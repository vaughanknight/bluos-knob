// BLE HID host adapter for the Anticater knob (HOGP central via NimBLE).
// The NimBLE callbacks only enqueue raw reports; all decoding/policy runs in the app task.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "knob_event.h"

enum class KnobLink { Idle, Scanning, Connecting, Connected, Disconnected };

const char* knob_link_name(KnobLink link);

class BleKnob {
   public:
    // name_hint: case-insensitive substring of the advertised name (e.g. "ANTICATER").
    void begin(QueueHandle_t queue, const char* name_hint);
    void loop();
    void clear_bonds();

    KnobLink link() const;
    uint32_t overflow_count() const;
    uint32_t reconnects() const;
    String peer_name() const;
};
