/**
 * @file screen_menu_advanced_settings.cpp
 */

#include "screen_menu_advanced_settings.hpp"

#include <marlin_client.hpp>

ScreenMenuAdvancedSettings::ScreenMenuAdvancedSettings()
    : detail::ScreenMenuAdvancedSettings(_(label)) {
    const bool editing_enabled = !marlin_client::is_printing();

    Item<MI_ADV_STEPS_PER_UNIT_X>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_Y>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_Z>().set_enabled(editing_enabled);
    Item<MI_ADV_STEPS_PER_UNIT_E>().set_enabled(editing_enabled);
}
