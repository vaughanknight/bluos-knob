#include "ble_knob.h"

#include <NimBLEDevice.h>

#include <atomic>

// Connection strategy:
// - Not bonded: scan (controller duplicate filter OFF, so a device seen once is
//   reported again on later scans) for an advertiser whose name matches the hint.
// - Bonded: skip scanning and keep an async direct connect pending to the bonded
//   address; it completes whenever the knob wakes and advertises.
// NimBLE callbacks run on the host task and only set flags or enqueue reports;
// blocking work (encryption, discovery, subscribe) runs from loop().

namespace {

constexpr uint16_t kHidService = 0x1812;
constexpr uint16_t kHidReport = 0x2A4D;
constexpr uint32_t kScanMs = 10000;
constexpr uint32_t kConnectTimeoutMs = 30000;
constexpr uint32_t kRetryMs = 500;

QueueHandle_t g_queue = nullptr;
String g_name_hint;
std::atomic<KnobLink> g_link{KnobLink::Idle};
std::atomic<uint32_t> g_overflow{0};
std::atomic<uint32_t> g_reconnects{0};
std::atomic<bool> g_found{false};
std::atomic<bool> g_diag{false};
std::atomic<bool> g_link_up{false};
std::atomic<bool> g_attempt_ended{false};
NimBLEAddress g_target;
String g_peer_name;
NimBLEClient* g_client = nullptr;
uint32_t g_next_attempt_ms = 0;

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
        if (g_diag) {
            if (dev->getRSSI() < -75 && !name_matches(dev->getName()) &&
                !NimBLEDevice::isBonded(dev->getAddress()))
                return;
            Serial.printf("{\"adv\":\"%s\",\"type\":%d,\"adv_type\":%d,\"name\":\"%s\",\"rssi\":%d,"
                          "\"connectable\":%d,\"bonded\":%d}\n",
                          dev->getAddress().toString().c_str(), dev->getAddress().getType(),
                          dev->getAdvType(), dev->getName().c_str(), dev->getRSSI(),
                          dev->isConnectable(), NimBLEDevice::isBonded(dev->getAddress()));
            return;
        }
        if (g_found || !name_matches(dev->getName())) return;
        g_target = dev->getAddress();
        g_peer_name = dev->getName().c_str();
        g_found = true;
        NimBLEDevice::getScan()->stop();
    }
} g_scan_callbacks;

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override { g_link_up = true; }
    void onConnectFail(NimBLEClient*, int reason) override {
        Serial.printf("{\"knob\":\"connect_failed\",\"reason\":%d}\n", reason);
        g_attempt_ended = true;
    }
    void onDisconnect(NimBLEClient*, int reason) override {
        Serial.printf("{\"knob\":\"disconnected\",\"reason\":%d}\n", reason);
        g_link_up = false;
        g_attempt_ended = true;
    }
} g_client_callbacks;

NimBLEClient* client() {
    if (g_client == nullptr) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(&g_client_callbacks, false);
        g_client->setConnectTimeout(kConnectTimeoutMs);
    }
    return g_client;
}

bool bonded_target(NimBLEAddress* out) {
    if (NimBLEDevice::getNumBonds() == 0) return false;
    *out = NimBLEDevice::getBondedAddress(0);
    return true;
}

void start_connect(const NimBLEAddress& addr) {
    g_target = addr;
    g_link_up = false;
    g_attempt_ended = false;
    g_link = KnobLink::Connecting;
    if (!client()->connect(addr, true, true)) {  // async: completes via callbacks
        g_attempt_ended = true;
    }
}

bool secure_and_subscribe() {
    NimBLEClient* c = client();
    if (!c->secureConnection()) {
        Serial.println("{\"knob\":\"secure_failed\"}");
        return false;
    }
    NimBLERemoteService* hid = c->getService(NimBLEUUID(kHidService));
    if (hid == nullptr) {
        Serial.println("{\"knob\":\"no_hid_service\"}");
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
    return subscribed > 0;
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
    scan->setDuplicateFilter(0);
    scan->setMaxResults(0);
    scan->setInterval(100);
    scan->setWindow(99);
    g_link = KnobLink::Disconnected;
}

void BleKnob::loop() {
    const uint32_t now = millis();
    switch (g_link.load()) {
        case KnobLink::Connected:
            if (g_attempt_ended) {  // disconnected
                g_link = KnobLink::Disconnected;
                g_next_attempt_ms = now + kRetryMs;
            }
            return;
        case KnobLink::Connecting:
            if (g_link_up) {
                if (secure_and_subscribe()) {
                    g_link = KnobLink::Connected;
                    g_reconnects++;
                } else {
                    client()->disconnect();
                    g_link = KnobLink::Disconnected;
                    g_next_attempt_ms = now + kRetryMs;
                }
            } else if (g_attempt_ended) {
                g_link = KnobLink::Disconnected;
                g_next_attempt_ms = now + kRetryMs;
            }
            return;
        case KnobLink::Scanning:
            if (g_found) {
                start_connect(g_target);
            } else if (!NimBLEDevice::getScan()->isScanning()) {
                g_link = KnobLink::Disconnected;
                g_next_attempt_ms = now + kRetryMs;
            }
            return;
        case KnobLink::Idle:
        case KnobLink::Disconnected:
            break;
    }
    if (static_cast<int32_t>(now - g_next_attempt_ms) < 0) return;
    NimBLEAddress bonded;
    if (bonded_target(&bonded)) {
        start_connect(bonded);
        return;
    }
    g_found = false;
    g_link = KnobLink::Scanning;
    if (!NimBLEDevice::getScan()->start(kScanMs, false, true)) {
        g_link = KnobLink::Disconnected;
        g_next_attempt_ms = now + kRetryMs;
    }
}

void BleKnob::diag_scan(uint32_t ms) {
    NimBLEClient* c = client();
    if (g_link == KnobLink::Connecting) c->cancelConnect();
    g_diag = true;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(false);
    Serial.printf("{\"diag\":\"scan_start\",\"ms\":%lu,\"bonds\":%d}\n", static_cast<unsigned long>(ms),
                  NimBLEDevice::getNumBonds());
    scan->getResults(ms, false);  // blocking
    scan->setActiveScan(true);
    g_diag = false;
    Serial.println("{\"diag\":\"scan_end\"}");
    g_link = KnobLink::Disconnected;
    g_next_attempt_ms = 0;
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
