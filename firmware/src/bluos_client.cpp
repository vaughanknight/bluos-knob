#include "bluos_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/semphr.h>

#include "timing.h"

namespace {

constexpr uint32_t kPollMs = 2000;
constexpr uint32_t kHttpTimeoutMs = 2000;

std::string g_host;
int g_port = 11000;
AmpState g_state;
SemaphoreHandle_t g_lock = nullptr;

bool http_get(const char* path, std::string* body, std::string* error) {
    HTTPClient http;
    const String url = String("http://") + g_host.c_str() + ":" + g_port + path;
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(url)) {
        *error = "bad amp address";
        return false;
    }
    const int code = http.GET();
    if (code != 200) {
        *error = code < 0 ? std::string("can't reach amp ") + g_host : "amp replied HTTP " + std::to_string(code);
        http.end();
        return false;
    }
    *body = http.getString().c_str();
    http.end();
    return true;
}

void poll_task(void*) {
    knob::Backoff backoff(kPollMs, 30000);
    for (;;) {
        uint32_t wait = kPollMs;
        if (WiFi.status() == WL_CONNECTED) {
            std::string body, error;
            knob::VolumeInfo volume;
            knob::StatusInfo status;
            bool ok = http_get("/Volume", &body, &error);
            if (ok) {
                volume = knob::parse_volume(body);
                if (!volume.ok) {
                    ok = false;
                    error = "amp sent an unreadable volume";
                }
            }
            if (ok) {
                ok = http_get("/Status", &body, &error);
                if (ok) {
                    status = knob::parse_status(body);
                    if (!status.ok) {
                        ok = false;
                        error = "amp sent an unreadable status";
                    }
                }
            }
            xSemaphoreTake(g_lock, portMAX_DELAY);
            if (ok) {
                g_state.reachable = true;
                g_state.have_data = true;
                g_state.volume = volume;
                g_state.status = status;
                g_state.error.clear();
                g_state.updated_ms = millis();
                g_state.failures = 0;
                backoff.on_success();
            } else {
                g_state.reachable = false;
                g_state.error = error;
                g_state.failures++;
                wait = backoff.on_failure();
            }
            xSemaphoreGive(g_lock);
            if (!ok) {
                std::string safe;
                for (const char c : error) safe += (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) ? '?' : c;
                Serial.printf("{\"amp\":\"unreachable\",\"error\":\"%s\",\"retry_ms\":%lu}\n", safe.c_str(),
                              static_cast<unsigned long>(wait));
            }
        }
        else {
            xSemaphoreTake(g_lock, portMAX_DELAY);
            g_state.reachable = false;  // never report stale data as live while Wi-Fi is down
            g_state.error = "Wi-Fi down";
            xSemaphoreGive(g_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

}  // namespace

void bluos_start(const std::string& host, int port) {
    g_host = host;
    g_port = port;
    g_lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(poll_task, "bluos", 8192, nullptr, 1, nullptr, 1);
}

AmpState bluos_snapshot() {
    AmpState copy;
    if (g_lock == nullptr) return copy;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    copy = g_state;
    xSemaphoreGive(g_lock);
    return copy;
}
