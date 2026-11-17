// Read-only BluOS poller. Runs in its own FreeRTOS task so HTTP timeouts never stall the UI or BLE.
#pragma once

#include <string>

#include "bluos_xml.h"

struct AmpState {
    bool reachable = false;
    bool have_data = false;
    knob::VolumeInfo volume;
    knob::StatusInfo status;
    std::string error;       // last failure, plain words
    uint32_t updated_ms = 0;  // millis() of the last fully successful poll; check age before acting
    uint32_t failures = 0;
};

void bluos_start(const std::string& host, int port);
AmpState bluos_snapshot();
