/**
 * @file screen_menu_advanced_settings.hpp
 */

#pragma once

#include "MItem_advanced_settings.hpp"
#include "screen_menu.hpp"

namespace detail {

using ScreenMenuAdvancedSettings = ScreenMenu<
    GuiDefaults::MenuFooter,
    MI_RETURN,
    MI_ADV_STEPS_PER_UNIT_X,
    MI_ADV_STEPS_PER_UNIT_Y,
    MI_ADV_STEPS_PER_UNIT_Z,
    MI_ADV_STEPS_PER_UNIT_E,
    MI_ADV_PREHEAT_FOR_UNLOADING>;

} // namespace detail

class ScreenMenuAdvancedSettings : public detail::ScreenMenuAdvancedSettings {
public:
    constexpr static const char *label = N_("ADVANCED SETTINGS");

    ScreenMenuAdvancedSettings();
};
