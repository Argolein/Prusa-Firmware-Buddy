/**
 * @file MItem_advanced_settings.cpp
 */

#include "MItem_advanced_settings.hpp"

#include "ScreenHandler.hpp"
#include <WindowMenuSpin.hpp>
#include <module/planner.h>
#include <module/prusa/homing_corexy.hpp>
#include <module/stepper/indirection.h>
#include <config_store/constants.hpp>
#include <config_store/store_instance.hpp>
#include <persistent_stores/store_instances/config_store/store_c_api.h>
#include <printers.h>
#include <stdint.h>

static constexpr NumericInputConfig steps_per_unit_spin_config = {
    .min_value = 1,
    .max_value = 1000,
    .step = 0.01F,
    .max_decimal_places = 2,
};

static constexpr NumericInputConfig motor_current_spin_config = {
    .min_value = 200,
    .max_value = 958,
    .step = 1,
    .unit = Unit::milliamper,
};

static constexpr NumericInputConfig homing_sensitivity_spin_config = {
    .min_value = -10,
    .max_value = 10,
    .step = 1,
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

static uint16_t get_current_ma_x() {
#if PRINTER_IS_PRUSA_COREONE() && AXIS_IS_TMC(X)
    return stepperX.getMilliamps();
#else
    return get_rms_current_ma_x();
#endif
}

static uint16_t get_current_ma_y() {
#if PRINTER_IS_PRUSA_COREONE() && AXIS_IS_TMC(Y)
    return stepperY.getMilliamps();
#else
    return get_rms_current_ma_y();
#endif
}

static uint16_t get_current_ma_z() {
#if PRINTER_IS_PRUSA_COREONE() && AXIS_IS_TMC(Z)
    return stepperZ.getMilliamps();
#else
    return get_rms_current_ma_z();
#endif
}

static uint16_t get_current_ma_e() {
#if PRINTER_IS_PRUSA_COREONE() && AXIS_IS_TMC(E0)
    return stepperE0.getMilliamps();
#else
    return get_rms_current_ma_e();
#endif
}

static bool motor_current_is_valid(const uint16_t current) {
    return current >= motor_current_spin_config.min_value && current <= motor_current_spin_config.max_value;
}

static bool current_change_invalidates_corexy_homing(const AxisEnum axis, const uint16_t current) {
#if HAS_PRECISE_HOMING_COREXY()
    switch (axis) {
    case X_AXIS:
        return current != get_rms_current_ma_x();
    case Y_AXIS:
        return current != get_rms_current_ma_y();
    default:
        return false;
    }
#else
    (void)axis;
    (void)current;
    return false;
#endif
}

static void store_motor_current(const AxisEnum axis, const uint16_t current) {
    if (!motor_current_is_valid(current)) {
        return;
    }

    const bool invalidate_corexy_homing = current_change_invalidates_corexy_homing(axis, current);

    switch (axis) {
    case X_AXIS:
        set_rms_current_ma_x(current);
        break;
    case Y_AXIS:
        set_rms_current_ma_y(current);
        break;
    case Z_AXIS:
        set_rms_current_ma_z(current);
        break;
    case E_AXIS:
        set_rms_current_ma_e(current);
        break;
    default:
        return;
    }

#if HAS_PRECISE_HOMING_COREXY()
    if (invalidate_corexy_homing) {
        corexy_clear_homing_calibration();
    }
#endif
}

static void apply_motor_current(const AxisEnum axis, const uint16_t current) {
    if (!motor_current_is_valid(current)) {
        return;
    }

#if PRINTER_IS_PRUSA_COREONE()
    switch (axis) {
    case X_AXIS:
    #if AXIS_IS_TMC(X)
        stepperX.rms_current(current);
    #endif
        break;
    case Y_AXIS:
    #if AXIS_IS_TMC(Y)
        stepperY.rms_current(current);
    #endif
        break;
    case Z_AXIS:
    #if AXIS_IS_TMC(Z)
        stepperZ.rms_current(current);
    #endif
        break;
    case E_AXIS:
    #if AXIS_IS_TMC(E0)
        stepperE0.rms_current(current);
    #endif
        break;
    default:
        return;
    }
#else
    (void)axis;
#endif
}

static void store_and_apply_motor_current(const AxisEnum axis, const uint16_t current) {
    store_motor_current(axis, current);
    apply_motor_current(axis, current);
}

static int16_t get_default_homing_sensitivity(const AxisEnum axis) {
    return axis == X_AXIS ? X_STALL_SENSITIVITY : Y_STALL_SENSITIVITY;
}

static int16_t get_homing_sensitivity(const AxisEnum axis) {
    const int16_t sensitivity = (axis == X_AXIS) ? config_store().homing_sens_x.get() : config_store().homing_sens_y.get();
    return sensitivity == config_store_ns::stallguard_sensitivity_unset ? get_default_homing_sensitivity(axis) : sensitivity;
}

static bool homing_sensitivity_is_valid(const int16_t sensitivity) {
    return sensitivity >= homing_sensitivity_spin_config.min_value && sensitivity <= homing_sensitivity_spin_config.max_value;
}

static void store_homing_sensitivity(const AxisEnum axis, const int16_t sensitivity) {
    if (!homing_sensitivity_is_valid(sensitivity)) {
        return;
    }

    switch (axis) {
    case X_AXIS:
        config_store().homing_sens_x.set(sensitivity);
        break;
    case Y_AXIS:
        config_store().homing_sens_y.set(sensitivity);
        break;
    default:
        return;
    }

#if HAS_PRECISE_HOMING_COREXY()
    corexy_clear_homing_calibration();
#endif
}

static void apply_homing_sensitivity(const AxisEnum axis, const int16_t sensitivity) {
    if (!homing_sensitivity_is_valid(sensitivity)) {
        return;
    }

#if PRINTER_IS_PRUSA_COREONE()
    switch (axis) {
    case X_AXIS:
    #if X_SENSORLESS
        stepperX.stall_sensitivity(sensitivity);
    #endif
        break;
    case Y_AXIS:
    #if Y_SENSORLESS
        stepperY.stall_sensitivity(sensitivity);
    #endif
        break;
    default:
        return;
    }
#else
    (void)axis;
    (void)sensitivity;
#endif
}

static void store_and_apply_homing_sensitivity(const AxisEnum axis, const int16_t sensitivity) {
    store_homing_sensitivity(axis, sensitivity);
    apply_homing_sensitivity(axis, sensitivity);
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

MI_ADV_CURRENT_X::MI_ADV_CURRENT_X()
    : WiSpin(get_current_ma_x(), motor_current_spin_config, _("X-axis current")) {}

void MI_ADV_CURRENT_X::Store() {
    store_and_apply_motor_current(X_AXIS, static_cast<uint16_t>(GetVal()));
}

void MI_ADV_CURRENT_X::OnClick() {
    Store();
}

MI_ADV_CURRENT_Y::MI_ADV_CURRENT_Y()
    : WiSpin(get_current_ma_y(), motor_current_spin_config, _("Y-axis current")) {}

void MI_ADV_CURRENT_Y::Store() {
    store_and_apply_motor_current(Y_AXIS, static_cast<uint16_t>(GetVal()));
}

void MI_ADV_CURRENT_Y::OnClick() {
    Store();
}

MI_ADV_CURRENT_Z::MI_ADV_CURRENT_Z()
    : WiSpin(get_current_ma_z(), motor_current_spin_config, _("Z-axis current")) {}

void MI_ADV_CURRENT_Z::Store() {
    store_and_apply_motor_current(Z_AXIS, static_cast<uint16_t>(GetVal()));
}

void MI_ADV_CURRENT_Z::OnClick() {
    Store();
}

MI_ADV_CURRENT_E::MI_ADV_CURRENT_E()
    : WiSpin(get_current_ma_e(), motor_current_spin_config, _("E-axis current")) {}

void MI_ADV_CURRENT_E::Store() {
    store_and_apply_motor_current(E_AXIS, static_cast<uint16_t>(GetVal()));
}

void MI_ADV_CURRENT_E::OnClick() {
    Store();
}

MI_ADV_CURRENT_RESET_DEFAULTS::MI_ADV_CURRENT_RESET_DEFAULTS()
    : IWindowMenuItem(_("Reset to defaults")) {}

void MI_ADV_CURRENT_RESET_DEFAULTS::click([[maybe_unused]] IWindowMenu &window_menu) {
    Screens::Access()->Get()->WindowEvent(nullptr, GUI_event_t::CHILD_CLICK, reinterpret_cast<void *>(static_cast<intptr_t>(AdvancedSettingsClickCommand::Reset_motor_currents)));
}

MI_ADV_HOMING_SENS_X::MI_ADV_HOMING_SENS_X()
    : WiSpin(get_homing_sensitivity(X_AXIS), homing_sensitivity_spin_config, _("X-axis sensitivity")) {}

void MI_ADV_HOMING_SENS_X::Store() {
    store_and_apply_homing_sensitivity(X_AXIS, static_cast<int16_t>(GetVal()));
}

void MI_ADV_HOMING_SENS_X::OnClick() {
    Store();
}

MI_ADV_HOMING_SENS_Y::MI_ADV_HOMING_SENS_Y()
    : WiSpin(get_homing_sensitivity(Y_AXIS), homing_sensitivity_spin_config, _("Y-axis sensitivity")) {}

void MI_ADV_HOMING_SENS_Y::Store() {
    store_and_apply_homing_sensitivity(Y_AXIS, static_cast<int16_t>(GetVal()));
}

void MI_ADV_HOMING_SENS_Y::OnClick() {
    Store();
}

MI_ADV_HOMING_SENS_RESET_DEFAULTS::MI_ADV_HOMING_SENS_RESET_DEFAULTS()
    : IWindowMenuItem(_("Reset to defaults")) {}

void MI_ADV_HOMING_SENS_RESET_DEFAULTS::click([[maybe_unused]] IWindowMenu &window_menu) {
    Screens::Access()->Get()->WindowEvent(nullptr, GUI_event_t::CHILD_CLICK, reinterpret_cast<void *>(static_cast<intptr_t>(AdvancedSettingsClickCommand::Reset_homing_sensitivity)));
}

MI_ADV_PREHEAT_FOR_UNLOADING::MI_ADV_PREHEAT_FOR_UNLOADING()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().preheat_for_unloading.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_PREHEAT_FOR_UNLOADING::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().preheat_for_unloading.set(value());
}

MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE::MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().cooldown_after_loading_when_idle.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().cooldown_after_loading_when_idle.set(value());
}

MI_ADV_AUTO_Z_ALIGN_HOMING::MI_ADV_AUTO_Z_ALIGN_HOMING()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().auto_z_align_homing.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_AUTO_Z_ALIGN_HOMING::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().auto_z_align_homing.set(value());
}
