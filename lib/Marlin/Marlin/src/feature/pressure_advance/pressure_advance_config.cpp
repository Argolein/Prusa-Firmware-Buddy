#include "pressure_advance.hpp"
#include "../precise_stepping/precise_stepping.hpp"

namespace pressure_advance {

static Config e_axis_config;

static bool is_pressure_advance_generator_enabled() {
    return PreciseStepping::physical_axis_step_generator_types & PRESSURE_ADVANCE_STEP_GENERATOR_E;
}

static void apply_axis_e_config() {
    if (e_axis_config.pressure_advance > 0.f) {
        PressureAdvance::pressure_advance_params = create_pressure_advance_params(e_axis_config);
        PreciseStepping::physical_axis_step_generator_types |= PRESSURE_ADVANCE_STEP_GENERATOR_E;
    } else {
        PreciseStepping::physical_axis_step_generator_types &= ~PRESSURE_ADVANCE_STEP_GENERATOR_E;
    }

    PreciseStepping::update_maximum_lookback_time();
}

bool axis_e_config_supports_runtime_update(const Config &config) {
    return e_axis_config.smooth_time == config.smooth_time
        && PreciseStepping::processing()
        && is_pressure_advance_generator_enabled()
        && e_axis_config.pressure_advance > 0.f
        && config.pressure_advance > 0.f;
}

void init() {
    // stub
}

const Config &get_axis_e_config() {
    return e_axis_config;
}

void set_axis_e_config(const Config &config) {
    // ensure we're not attempting to change global parameters within a guard
    assert(!PressureAdvanceDisabler::is_active());

    const bool runtime_update = axis_e_config_supports_runtime_update(config);

    if (!runtime_update) {
        // Generator topology and lookback timing may change, so require empty queues.
        assert(PreciseStepping::move_segment_queue_size() == 0);
    }

    e_axis_config = config;

    if (runtime_update) {
        return;
    }

    apply_axis_e_config();
}

void sync_axis_e_config_if_idle() {
    if (PreciseStepping::processing()) {
        return;
    }

    assert(PreciseStepping::move_segment_queue_size() == 0);
    apply_axis_e_config();
}

} // namespace pressure_advance
