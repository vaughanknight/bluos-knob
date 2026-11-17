// Persists knob::Config in NVS (ESP32 Preferences).
#pragma once

#include "config.h"

void config_load(knob::Config& config);
bool config_save(const knob::Config& config);
