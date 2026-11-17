#include "amp_control.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <string>

#include "config_store.h"

namespace {

constexpr uint32_t kMaxEventAgeMs = 1000;
constexpr uint32_t kHttpTimeoutMs = 2000;
constexpr size_t kQueueDepth = 8;

struct Pending {
    knob::Action action;
    uint32_t event_ms;
    uint32_t generation;
};

class HttpAmp : public knob::AmpTransport {
   public:
    HttpAmp(const std::string& host, int port) : base_("http://" + host + ":" + std::to_string(port)) {}
    bool get(const std::string& path, std::string* body) override {
        if (WiFi.status() != WL_CONNECTED) return false;
        HTTPClient http;
        http.setConnectTimeout(kHttpTimeoutMs);
        http.setTimeout(kHttpTimeoutMs);
        if (!http.begin((base_ + path).c_str())) return false;
        const int code = http.GET();
        if (code != 200) {
            http.end();
            return false;
        }
        *body = http.getString().c_str();
        http.end();
        return true;
    }

   private:
    std::string base_;
};

knob::Config g_config;
HttpAmp* g_amp = nullptr;
QueueHandle_t g_queue = nullptr;
SemaphoreHandle_t g_lock = nullptr;
std::atomic<bool> g_live{false};  // restored from NVS by control_start
std::atomic<uint32_t> g_generation{0};
std::atomic<uint32_t> g_dropped{0};
ControlResult g_last;

std::string json_safe(const std::string& in) {
    std::string out;
    for (const char c : in) out += (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) ? '?' : c;
    return out;
}

void control_task(void*) {
    Pending p;
    for (;;) {
        if (xQueueReceive(g_queue, &p, portMAX_DELAY) != pdTRUE) continue;
        const uint32_t now = millis();
        if (p.generation != g_generation || knob::event_is_stale(p.event_ms, now, kMaxEventAgeMs)) {
            g_dropped++;
            Serial.printf("{\"control\":\"dropped\",\"action\":\"%s\",\"age_ms\":%lu}\n", knob::action_name(p.action),
                          static_cast<unsigned long>(now - p.event_ms));
            continue;
        }
        const bool live = g_live;
        const knob::ExecResult r = knob::execute(knob::plan_command(p.action, g_config), g_config, *g_amp, live);
        Serial.printf(
            "{\"control\":\"%s\",\"command\":\"%s\",\"live\":%d,\"before_db\":%s,\"after_db\":%s,\"cap\":%d,"
            "\"writes\":%d,\"message\":\"%s\"}\n",
            knob::outcome_name(r.outcome), knob::command_name(r.kind), live,
            r.has_before ? String(r.before_db, 1).c_str() : "null", r.has_after ? String(r.after_db, 1).c_str() : "null",
            r.cap_blocked, r.writes, json_safe(r.message).c_str());
        xSemaphoreTake(g_lock, portMAX_DELAY);
        g_last.valid = true;
        g_last.at_ms = millis();
        g_last.result = r;
        xSemaphoreGive(g_lock);
    }
}

}  // namespace

void control_start(const knob::Config& config) {
    g_config = config;
    g_live = live_load();
    g_amp = new HttpAmp(config.host, config.port);
    g_queue = xQueueCreate(kQueueDepth, sizeof(Pending));
    g_lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(control_task, "control", 8192, nullptr, 2, nullptr, 1);
}

void control_submit(knob::Action action, uint32_t event_ms) {
    if (g_queue == nullptr) return;
    if (knob::plan_command(action, g_config).kind == knob::CommandKind::Ignore) return;
    const Pending p{action, event_ms, g_generation};
    if (xQueueSend(g_queue, &p, 0) != pdTRUE) g_dropped++;
}

void control_flush() {
    g_generation++;
    if (g_queue != nullptr) xQueueReset(g_queue);
}

void control_set_live(bool live) {
    control_flush();
    g_live = live;
    bool saved = live_save(live);
    if (!saved) saved = live_save(live);  // one retry: an unsaved mode would come back after a restart
    Serial.printf("{\"live\":%s,\"remembered\":%s}\n", live ? "true" : "false", saved ? "true" : "false");
}

bool control_live() { return g_live; }

ControlResult control_last() {
    ControlResult copy;
    if (g_lock == nullptr) return copy;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    copy = g_last;
    xSemaphoreGive(g_lock);
    return copy;
}

uint32_t control_dropped() { return g_dropped; }
