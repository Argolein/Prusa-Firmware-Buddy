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
    MI_ADV_PREHEAT_FOR_UNLOADING,
    MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE,
    MI_ADV_AUTO_Z_ALIGN_HOMING>;

} // namespace detail

class ScreenMenuAdvancedSettings : public detail::ScreenMenuAdvancedSettings {
public:
    constexpr static const char *label = N_("ADVANCED SETTINGS");

    ScreenMenuAdvancedSettings();
};
