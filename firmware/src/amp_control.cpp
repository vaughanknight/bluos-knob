#include "amp_control.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <string>
#include <vector>

#include "config_store.h"

namespace {

constexpr uint32_t kMaxEventAgeMs = 1000;
constexpr uint32_t kHttpTimeoutMs = 2000;
constexpr size_t kQueueDepth = 32;  // bursts are coalesced, so depth only bounds memory

struct Pending {
    knob::Action action;
    uint32_t event_ms;
    uint32_t generation;
};

// One kept-alive connection to the amp. A read that fails (e.g. the amp closed an idle connection) is
// retried once on a fresh connection; writes are never retried (they may have applied).
class HttpAmp : public knob::AmpTransport {
   public:
    HttpAmp(const std::string& host, int port) : base_("http://" + host + ":" + std::to_string(port)) {
        http_.setReuse(true);
        http_.setConnectTimeout(kHttpTimeoutMs);
        http_.setTimeout(kHttpTimeoutMs);
    }
    bool get(const std::string& path, std::string* body) override {
        if (WiFi.status() != WL_CONNECTED) return false;
        if (once(path, body)) return true;
        const bool is_read = path == "/Volume" || path == "/Status" || path.rfind("/RadioBrowse", 0) == 0;
        if (!is_read) return false;
        tcp_.stop();
        return once(path, body);
    }

   private:
    bool once(const std::string& path, std::string* body) {
        if (!http_.begin(tcp_, (base_ + path).c_str())) return false;
        const int code = http_.GET();
        const bool ok = code == 200;
        if (ok) *body = http_.getString().c_str();
        http_.end();  // keeps the TCP connection open (setReuse)
        if (code < 0) tcp_.stop();
        return ok;
    }

    std::string base_;
    WiFiClient tcp_;
    HTTPClient http_;
};

knob::Config g_config;
HttpAmp* g_amp = nullptr;
QueueHandle_t g_queue = nullptr;
SemaphoreHandle_t g_lock = nullptr;
std::atomic<bool> g_live{false};  // restored from NVS by control_start
std::atomic<uint32_t> g_generation{0};
std::atomic<uint32_t> g_dropped{0};
std::atomic<bool> g_executing{false};
ControlResult g_last;

std::string json_safe(const std::string& in) {
    std::string out;
    for (const char c : in) out += (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) ? '?' : c;
    return out;
}

void log_result(const knob::ExecResult& r, bool live, int clicks) {
    Serial.printf(
        "{\"control\":\"%s\",\"command\":\"%s\",\"live\":%d,\"clicks\":%d,\"before_db\":%s,\"after_db\":%s,"
        "\"cap\":%d,\"writes\":%d,\"message\":\"%s\"}\n",
        knob::outcome_name(r.outcome), knob::command_name(r.kind), live, clicks,
        r.has_before ? String(r.before_db, 1).c_str() : "null", r.has_after ? String(r.after_db, 1).c_str() : "null",
        r.cap_blocked, r.writes, json_safe(r.message).c_str());
}

struct Segment {
    std::vector<knob::Action> actions;  // a run of volume clicks, or one barrier action
    uint32_t newest_ms = 0;
};

bool is_volume(knob::Action a) { return a == knob::Action::VolumeUp || a == knob::Action::VolumeDown; }

// Takes everything pending, drops stale/superseded clicks, coalesces runs of volume clicks into one net
// step, and executes in order. Before EACH command it re-checks the generation (live off / knob reconnect
// abandons the rest of the burst), re-reads the live flag, and re-applies the 1 s staleness rule.
void control_task(void*) {
    Pending p;
    for (;;) {
        if (xQueueReceive(g_queue, &p, portMAX_DELAY) != pdTRUE) continue;
        g_executing = true;
        const uint32_t generation = g_generation;
        std::vector<Segment> segments;
        int clicks = 0;
        do {
            const uint32_t now = millis();
            if (p.generation != generation || knob::event_is_stale(p.event_ms, now, kMaxEventAgeMs)) {
                g_dropped++;
                Serial.printf("{\"control\":\"dropped\",\"action\":\"%s\",\"age_ms\":%lu}\n",
                              knob::action_name(p.action), static_cast<unsigned long>(now - p.event_ms));
                continue;
            }
            clicks++;
            if (segments.empty() || !is_volume(p.action) || !is_volume(segments.back().actions.front()))
                segments.push_back({});
            segments.back().actions.push_back(p.action);
            segments.back().newest_ms = p.event_ms;
        } while (xQueueReceive(g_queue, &p, 0) == pdTRUE);

        for (size_t i = 0; i < segments.size(); ++i) {
            const uint32_t now = millis();
            if (g_generation != generation || knob::event_is_stale(segments[i].newest_ms, now, kMaxEventAgeMs)) {
                const size_t left = segments.size() - i;
                g_dropped += left;
                Serial.printf("{\"control\":\"abandoned\",\"segments\":%u,\"reason\":\"%s\"}\n",
                              static_cast<unsigned>(left), g_generation != generation ? "mode_or_link_change" : "stale");
                break;
            }
            for (const knob::Command& command : knob::coalesce(segments[i].actions, g_config)) {
                const bool live = g_live;
                const knob::ExecResult r = knob::execute(command, g_config, *g_amp, live);
                log_result(r, live, clicks);
                xSemaphoreTake(g_lock, portMAX_DELAY);
                g_last.valid = true;
                g_last.at_ms = millis();
                g_last.result = r;
                xSemaphoreGive(g_lock);
            }
        }
        g_executing = false;
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

bool control_busy() { return g_executing || (g_queue != nullptr && uxQueueMessagesWaiting(g_queue) > 0); }
