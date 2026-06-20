/**
 * @file MItem_advanced_settings.cpp
 */

#include "MItem_advanced_settings.hpp"

#include <WindowMenuSpin.hpp>
#include <module/planner.h>
#include <persistent_stores/store_instances/config_store/store_c_api.h>

static constexpr NumericInputConfig steps_per_unit_spin_config = {
    .min_value = 1,
    .max_value = 1000,
    .step = 0.01F,
    .max_decimal_places = 2,
};

static void store_and_apply_steps_per_unit(const AxisEnum axis, const float steps) {
    if (steps < steps_per_unit_spin_config.min_value || steps > steps_per_unit_spin_config.max_value) {
        return;
    }

    planner.synchronize();
    if (planner.draining()) {
        return;
    }

    switch (axis) {
    case X_AXIS:
        set_steps_per_unit_x(steps);
        break;
    case Y_AXIS:
        set_steps_per_unit_y(steps);
        break;
    case Z_AXIS:
        set_steps_per_unit_z(steps);
        break;
    case E_AXIS:
        set_steps_per_unit_e(steps);
        break;
    default:
        return;
    }

    auto settings = planner.user_settings;
    settings.axis_steps_per_mm[axis] = steps;
    settings.axis_msteps_per_mm[axis] = steps * PLANNER_STEPS_MULTIPLIER;
    planner.apply_settings(settings);
    planner.refresh_positioning();
}

MI_ADV_STEPS_PER_UNIT_X::MI_ADV_STEPS_PER_UNIT_X()
    : WiSpin(get_steps_per_unit_x(), steps_per_unit_spin_config, _("X-axis steps/mm")) {}

void MI_ADV_STEPS_PER_UNIT_X::OnClick() {
    store_and_apply_steps_per_unit(X_AXIS, GetVal());
}

MI_ADV_STEPS_PER_UNIT_Y::MI_ADV_STEPS_PER_UNIT_Y()
    : WiSpin(get_steps_per_unit_y(), steps_per_unit_spin_config, _("Y-axis steps/mm")) {}

void MI_ADV_STEPS_PER_UNIT_Y::OnClick() {
    store_and_apply_steps_per_unit(Y_AXIS, GetVal());
}

MI_ADV_STEPS_PER_UNIT_Z::MI_ADV_STEPS_PER_UNIT_Z()
    : WiSpin(get_steps_per_unit_z(), steps_per_unit_spin_config, _("Z-axis steps/mm")) {}

void MI_ADV_STEPS_PER_UNIT_Z::OnClick() {
    store_and_apply_steps_per_unit(Z_AXIS, GetVal());
}

MI_ADV_STEPS_PER_UNIT_E::MI_ADV_STEPS_PER_UNIT_E()
    : WiSpin(get_steps_per_unit_e(), steps_per_unit_spin_config, _("E-axis steps/mm")) {}

void MI_ADV_STEPS_PER_UNIT_E::OnClick() {
    store_and_apply_steps_per_unit(E_AXIS, GetVal());
}

MI_ADV_PREHEAT_FOR_UNLOADING::MI_ADV_PREHEAT_FOR_UNLOADING()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().preheat_for_unloading.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_PREHEAT_FOR_UNLOADING::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().preheat_for_unloading.set(value());
}
