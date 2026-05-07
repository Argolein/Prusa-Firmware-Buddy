/**
 * @file screen_menu_advanced_settings.cpp
 */

#include "screen_menu_advanced_settings.hpp"

#include <marlin_client.hpp>
#include <config_store/constants.hpp>
#include <config_store/store_instance.hpp>
#include <persistent_stores/store_instances/config_store/store_c_api.h>
#include <stdint.h>

ScreenMenuAdvancedSettings::ScreenMenuAdvancedSettings()
    : detail::ScreenMenuAdvancedSettings(_(label)) {}

ScreenMenuAdvancedStepsPerMm::ScreenMenuAdvancedStepsPerMm()
    : detail::ScreenMenuAdvancedStepsPerMm(_(label)) {
    const bool editing_enabled = !marlin_client::is_printing();

    Item<MI_ADV_STEPS_PER_UNIT_X>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_Y>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_Z>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_E>().set_enabled(editing_enabled);
}

ScreenMenuAdvancedMotorCurrents::ScreenMenuAdvancedMotorCurrents()
    : detail::ScreenMenuAdvancedMotorCurrents(_(label)) {
    const bool editing_enabled = !marlin_client::is_printing();

    Item<MI_ADV_CURRENT_X>().set_enabled(editing_enabled);
    Item<MI_ADV_CURRENT_Y>().set_enabled(editing_enabled);
    Item<MI_ADV_CURRENT_Z>().set_enabled(editing_enabled);
    Item<MI_ADV_CURRENT_E>().set_enabled(editing_enabled);
    Item<MI_ADV_CURRENT_RESET_DEFAULTS>().set_enabled(editing_enabled);
}

void ScreenMenuAdvancedMotorCurrents::windowEvent(window_t *sender, GUI_event_t ev, void *param) {
    if (ev != GUI_event_t::CHILD_CLICK) {
        ScreenMenu::windowEvent(sender, ev, param);
        return;
    }

    switch (static_cast<AdvancedSettingsClickCommand>(reinterpret_cast<intptr_t>(param))) {
    case AdvancedSettingsClickCommand::Reset_motor_currents:
        Item<MI_ADV_CURRENT_X>().SetVal(get_default_rms_current_ma_x());
        Item<MI_ADV_CURRENT_Y>().SetVal(get_default_rms_current_ma_y());
        Item<MI_ADV_CURRENT_Z>().SetVal(get_default_rms_current_ma_z());
        Item<MI_ADV_CURRENT_E>().SetVal(get_default_rms_current_ma_e());

        Item<MI_ADV_CURRENT_X>().Store();
        Item<MI_ADV_CURRENT_Y>().Store();
        Item<MI_ADV_CURRENT_Z>().Store();
        Item<MI_ADV_CURRENT_E>().Store();

        Invalidate();
        break;
    default:
        break;
    }
}

ScreenMenuAdvancedHomingSensitivity::ScreenMenuAdvancedHomingSensitivity()
    : detail::ScreenMenuAdvancedHomingSensitivity(_(label)) {
    const bool editing_enabled = !marlin_client::is_printing();

    Item<MI_ADV_HOMING_SENS_X>().set_enabled(editing_enabled);
    Item<MI_ADV_HOMING_SENS_Y>().set_enabled(editing_enabled);
    Item<MI_ADV_HOMING_SENS_RESET_DEFAULTS>().set_enabled(editing_enabled);
}

void ScreenMenuAdvancedHomingSensitivity::windowEvent(window_t *sender, GUI_event_t ev, void *param) {
    if (ev != GUI_event_t::CHILD_CLICK) {
        ScreenMenu::windowEvent(sender, ev, param);
        return;
    }

    switch (static_cast<AdvancedSettingsClickCommand>(reinterpret_cast<intptr_t>(param))) {
    case AdvancedSettingsClickCommand::Reset_homing_sensitivity:
        config_store().homing_sens_x.set(config_store_ns::stallguard_sensitivity_unset);
        config_store().homing_sens_y.set(config_store_ns::stallguard_sensitivity_unset);

        Item<MI_ADV_HOMING_SENS_X>().SetVal(X_STALL_SENSITIVITY);
        Item<MI_ADV_HOMING_SENS_Y>().SetVal(Y_STALL_SENSITIVITY);

        Item<MI_ADV_HOMING_SENS_X>().Store();
        Item<MI_ADV_HOMING_SENS_Y>().Store();

        config_store().homing_sens_x.set(config_store_ns::stallguard_sensitivity_unset);
        config_store().homing_sens_y.set(config_store_ns::stallguard_sensitivity_unset);

        Invalidate();
        break;
    default:
        break;
    }
}
