#include "ble_knob.h"

#include <NimBLEDevice.h>

#include <atomic>

namespace {

constexpr uint16_t kHidService = 0x1812;
constexpr uint16_t kHidReport = 0x2A4D;
constexpr uint32_t kScanMs = 10000;
constexpr uint32_t kBackoffMinMs = 1000;
constexpr uint32_t kBackoffMaxMs = 30000;

QueueHandle_t g_queue = nullptr;
String g_name_hint;
std::atomic<KnobLink> g_link{KnobLink::Idle};
std::atomic<uint32_t> g_overflow{0};
std::atomic<uint32_t> g_reconnects{0};
std::atomic<bool> g_have_target{false};
NimBLEAddress g_target;
String g_peer_name;
NimBLEClient* g_client = nullptr;
uint32_t g_next_attempt_ms = 0;
uint32_t g_backoff_ms = kBackoffMinMs;

void on_report(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    KnobEvent ev{};
    ev.at_ms = millis();
    ev.len = static_cast<uint8_t>(len > sizeof(ev.data) ? sizeof(ev.data) : len);
    memcpy(ev.data, data, ev.len);
    if (xQueueSend(g_queue, &ev, 0) != pdTRUE) g_overflow++;  // drop newest, never block the host task
}

bool name_matches(const std::string& name) {
    String n(name.c_str());
    n.toUpperCase();
    String hint = g_name_hint;
    hint.toUpperCase();
    return hint.length() > 0 && n.indexOf(hint) >= 0;
}

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        const bool bonded = NimBLEDevice::isBonded(dev->getAddress());
        if (!bonded && !name_matches(dev->getName())) return;
        g_target = dev->getAddress();
        g_peer_name = dev->getName().c_str();
        g_have_target = true;
        NimBLEDevice::getScan()->stop();
    }
    void onScanEnd(const NimBLEScanResults&, int) override {
        if (!g_have_target && g_link == KnobLink::Scanning) g_link = KnobLink::Disconnected;
    }
} g_scan_callbacks;

class ClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*, int reason) override {
        Serial.printf("{\"knob\":\"disconnected\",\"reason\":%d}\n", reason);
        g_link = KnobLink::Disconnected;
    }
} g_client_callbacks;

bool connect_and_subscribe() {
    if (g_client == nullptr) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(&g_client_callbacks, false);
        g_client->setConnectTimeout(10000);
    }
    if (!g_client->connect(g_target)) {
        Serial.println("{\"knob\":\"connect_failed\"}");
        return false;
    }
    if (!g_client->secureConnection()) {
        Serial.println("{\"knob\":\"secure_failed\"}");
        g_client->disconnect();
        return false;
    }
    NimBLERemoteService* hid = g_client->getService(NimBLEUUID(kHidService));
    if (hid == nullptr) {
        Serial.println("{\"knob\":\"no_hid_service\"}");
        g_client->disconnect();
        return false;
    }
    int subscribed = 0;
    for (NimBLERemoteCharacteristic* chr : hid->getCharacteristics(true)) {
        if (chr->getUUID() == NimBLEUUID(kHidReport) && chr->canNotify() &&
            chr->subscribe(true, on_report)) {
            subscribed++;
        }
    }
    Serial.printf("{\"knob\":\"connected\",\"name\":\"%s\",\"addr\":\"%s\",\"reports\":%d}\n",
                  g_peer_name.c_str(), g_target.toString().c_str(), subscribed);
    if (subscribed == 0) {
        g_client->disconnect();
        return false;
    }
    return true;
}

}  // namespace

const char* knob_link_name(KnobLink link) {
    switch (link) {
        case KnobLink::Idle: return "idle";
        case KnobLink::Scanning: return "scanning";
        case KnobLink::Connecting: return "connecting";
        case KnobLink::Connected: return "connected";
        case KnobLink::Disconnected: return "disconnected";
    }
    return "unknown";
}

void BleKnob::begin(QueueHandle_t queue, const char* name_hint) {
    g_queue = queue;
    g_name_hint = name_hint;
    NimBLEDevice::init("bluos-knob");
    NimBLEDevice::setSecurityAuth(true, false, true);  // bond, no MITM, secure connections
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_scan_callbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    g_link = KnobLink::Disconnected;
}

void BleKnob::loop() {
    const uint32_t now = millis();
    const KnobLink link = g_link;
    if (link == KnobLink::Connected || link == KnobLink::Connecting) return;
    if (link == KnobLink::Scanning) {
        if (!g_have_target) return;
        g_link = KnobLink::Connecting;
        if (connect_and_subscribe()) {
            g_link = KnobLink::Connected;
            g_backoff_ms = kBackoffMinMs;
            g_reconnects++;
        } else {
            g_have_target = false;
            g_link = KnobLink::Disconnected;
            g_next_attempt_ms = now + g_backoff_ms;
            g_backoff_ms = min(g_backoff_ms * 2, kBackoffMaxMs);
        }
        return;
    }
    if (static_cast<int32_t>(now - g_next_attempt_ms) < 0) return;
    g_have_target = false;
    g_link = KnobLink::Scanning;
    if (!NimBLEDevice::getScan()->start(kScanMs, false, true)) {
        g_link = KnobLink::Disconnected;
        g_next_attempt_ms = now + g_backoff_ms;
    }
}

void BleKnob::clear_bonds() {
    if (g_client != nullptr && g_client->isConnected()) g_client->disconnect();
    NimBLEDevice::deleteAllBonds();
    Serial.println("{\"knob\":\"bonds_cleared\"}");
}

KnobLink BleKnob::link() const { return g_link; }
uint32_t BleKnob::overflow_count() const { return g_overflow; }
uint32_t BleKnob::reconnects() const { return g_reconnects; }
String BleKnob::peer_name() const { return g_peer_name; }
