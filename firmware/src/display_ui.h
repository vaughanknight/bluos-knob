// On-device screen. Reads app state only; never drives control.
#pragma once

#include <Arduino.h>

#include <string>

#include "amp_control.h"
#include "ble_knob.h"
#include "bluos_client.h"
#include "decoder.h"

enum class WifiLink { NotConfigured, Connecting, Connected };

struct UiState {
    const char* board;
    KnobLink link;
    WifiLink wifi;
    AmpState amp;
    std::string fault;        // highest-priority problem in plain words, empty when healthy
    knob::Action last_action;
    bool show_action;         // briefly after a knob event
    bool dimmed;
    uint32_t events;
    bool live;                // session-only live amp control
    ControlResult control;    // last executed command
    bool show_result;         // briefly after a command
    double max_db;
};

void ui_begin();
void ui_render(const UiState& state);
