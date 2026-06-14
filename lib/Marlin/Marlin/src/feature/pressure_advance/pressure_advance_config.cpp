#include "pressure_advance.hpp"
#include "../precise_stepping/precise_stepping.hpp"

#include <atomic>

namespace pressure_advance {

static Config e_axis_config;

// Shadow of the per-move PA value to be applied to future moves enqueued by
// the planner. Written by the gcode thread (M572 / M900 handlers), read by the
// gcode thread again inside _populate_block. Relaxed ordering is sufficient
// because the planner queue itself serializes ordering across blocks.
static std::atomic<float> queued_value { 0.f };

// Tracks whether the PA step generator is currently armed (bit set in
// PreciseStepping::physical_axis_step_generator_types). Used by the hot path
// to detect non-zero → non-zero transitions, which can skip the planner
// flush. Going to PA value 0 (explicit M572 S0 or PressureAdvanceDisabler) is
// a *structural* disable that clears this flag and the generator bit so the
// FIR lookback time drops back to zero — critical for homing / probe paths
// that explicitly rely on PressureAdvanceDisabler to remove PA latency.
static bool generator_armed = false;

void init() {
    // stub
}

const Config &get_axis_e_config() {
    return e_axis_config;
}

float get_queued_value() {
    return queued_value.load(std::memory_order_relaxed);
}

void set_queued_value(float v) {
    queued_value.store(v, std::memory_order_relaxed);
}

bool can_use_queued_path(const Config &new_cfg) {
    // Hot path requirements:
    //  - FIR filter structure must be unchanged (smooth_time drives sampling
    //    rate, filter length, and lookback time — all global).
    //  - PA step generator must already be armed (i.e. we are between a
    //    structural M572 S>0 activation and the matching structural disable).
    //  - The new PA value must be > 0. M572 S0 is a documented disable and
    //    must take the structural path so the generator bit and lookback are
    //    cleared — homing / probe paths depend on this via
    //    PressureAdvanceDisabler.
    //  - PressureAdvanceDisabler must not be active. M572 should not race
    //    against the guard; if reachable at all (nested gcode), force the
    //    structural path so the guard's invariants are not violated.
    return generator_armed
        && new_cfg.pressure_advance > 0.f
        && new_cfg.smooth_time == e_axis_config.smooth_time
        && !PressureAdvanceDisabler::is_active();
}

void update_reported_value(float v) {
    // Keep get_axis_e_config().pressure_advance in sync with the queued value
    // so that M572 (no args) reports the value the user just set, even though
    // the actual per-move PA value is now carried on each move_t.
    e_axis_config.pressure_advance = v;
}

void set_axis_e_config(const Config &config) {
    // Structural path: caller (M572_internal / PressureAdvanceDisabler) is
    // responsible for ensuring the queue is empty before we touch the filter
    // structure and step-generator wiring.
    assert(PreciseStepping::move_segment_queue_size() == 0);

    // ensure we're not attempting to change global parameters within a guard
    assert(!PressureAdvanceDisabler::is_active());

    e_axis_config = config;

    if (config.pressure_advance > 0.f) {
        // Activation (first time, re-activation after a disable, or
        // smooth_time change while active): (re)compute filter params, arm
        // the generator bit, and seed the queued shadow so subsequent moves
        // carry this value.
        PressureAdvance::pressure_advance_params = create_pressure_advance_params(config);
        PreciseStepping::physical_axis_step_generator_types |= PRESSURE_ADVANCE_STEP_GENERATOR_E;
        generator_armed = true;
        set_queued_value(config.pressure_advance);
    } else {
        // Structural disable (M572 S0 or PressureAdvanceDisabler): clear the
        // generator bit so update_maximum_lookback_time() drops the FIR
        // lookback contribution back to zero. Homing / probe paths rely on
        // this to avoid the PA-induced motion latency.
        PreciseStepping::physical_axis_step_generator_types &= ~PRESSURE_ADVANCE_STEP_GENERATOR_E;
        generator_armed = false;
        set_queued_value(0.f);
    }

    PreciseStepping::update_maximum_lookback_time();
}

} // namespace pressure_advance
