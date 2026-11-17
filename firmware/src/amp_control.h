// Runs knob commands against the amp on its own task, through the guarded executor in knob_core.
#pragma once

#include <cstdint>

#include "amp_policy.h"
#include "config.h"
#include "decoder.h"

struct ControlResult {
    bool valid = false;
    uint32_t at_ms = 0;
    knob::ExecResult result;
};

// config is copied: control settings take effect after a reboot. Restores the remembered live flag.
void control_start(const knob::Config& config);
// Queue a gesture; dropped when stale, when the queue is full, or when superseded by a mode/link change.
void control_submit(knob::Action action, uint32_t event_ms);
// Invalidate everything queued (mode change, knob link change).
void control_flush();
void control_set_live(bool live);
bool control_live();
ControlResult control_last();
uint32_t control_dropped();
