// On-device screen. Reads app state only; never drives control.
#pragma once

#include <Arduino.h>

#include "ble_knob.h"
#include "decoder.h"

struct UiState {
    const char* board;
    KnobLink link;
    knob::Action last_action;
    uint32_t events;
    uint32_t overflow;
};

void ui_begin();
void ui_render(const UiState& state);
