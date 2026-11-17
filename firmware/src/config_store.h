// Persists knob::Config in NVS (ESP32 Preferences).
#pragma once

#include "config.h"

void config_load(knob::Config& config);
bool config_save(const knob::Config& config);

// Live amp control, remembered across restarts (owner decision 2026-11-17). Defaults to false.
bool live_load();
bool live_save(bool live);
