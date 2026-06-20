/**
 * @file screen_menu_advanced_settings.hpp
 */

#pragma once

#include "MItem_advanced_settings.hpp"
#include "MItem_menus.hpp"
#include "screen_menu.hpp"

namespace detail {

using ScreenMenuAdvancedSettings = ScreenMenu<
    GuiDefaults::MenuFooter,
    MI_RETURN,
    MI_ADVANCED_STEPS_PER_MM,
    MI_ADVANCED_MOTOR_CURRENTS,
    MI_ADVANCED_HOMING_SENSITIVITY,
    MI_ADV_PREHEAT_FOR_UNLOADING>;

using ScreenMenuAdvancedStepsPerMm = ScreenMenu<
    GuiDefaults::MenuFooter,
    MI_RETURN,
    MI_ADV_STEPS_PER_UNIT_X,
    MI_ADV_STEPS_PER_UNIT_Y,
    MI_ADV_STEPS_PER_UNIT_Z,
    MI_ADV_STEPS_PER_UNIT_E>;

using ScreenMenuAdvancedMotorCurrents = ScreenMenu<
    GuiDefaults::MenuFooter,
    MI_RETURN,
    MI_ADV_CURRENT_X,
    MI_ADV_CURRENT_Y,
    MI_ADV_CURRENT_Z,
    MI_ADV_CURRENT_E,
    MI_ADV_CURRENT_RESET_DEFAULTS>;

using ScreenMenuAdvancedHomingSensitivity = ScreenMenu<
    GuiDefaults::MenuFooter,
    MI_RETURN,
    MI_ADV_HOMING_SENS_X,
    MI_ADV_HOMING_SENS_Y,
    MI_ADV_HOMING_SENS_RESET_DEFAULTS>;

} // namespace detail

class ScreenMenuAdvancedSettings : public detail::ScreenMenuAdvancedSettings {
public:
    constexpr static const char *label = N_("ADVANCED SETTINGS");

    ScreenMenuAdvancedSettings();
};

class ScreenMenuAdvancedStepsPerMm : public detail::ScreenMenuAdvancedStepsPerMm {
public:
    constexpr static const char *label = N_("STEPS/MM");

    ScreenMenuAdvancedStepsPerMm();
};

class ScreenMenuAdvancedMotorCurrents : public detail::ScreenMenuAdvancedMotorCurrents {
public:
    constexpr static const char *label = N_("MOTOR CURRENTS");

    ScreenMenuAdvancedMotorCurrents();

protected:
    void windowEvent(window_t *sender, GUI_event_t ev, void *param) override;
};

class ScreenMenuAdvancedHomingSensitivity : public detail::ScreenMenuAdvancedHomingSensitivity {
public:
    constexpr static const char *label = N_("HOMING SENSITIVITY");

    ScreenMenuAdvancedHomingSensitivity();

protected:
    void windowEvent(window_t *sender, GUI_event_t ev, void *param) override;
};
