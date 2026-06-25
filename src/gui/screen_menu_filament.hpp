/**
 * @file screen_menu_filament.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"
#include "MItem_filament.hpp"
#include "MItem_menus.hpp"
#include "MItem_tools.hpp"
#include <screen/filament/screen_type_and_color.hpp>
#include <option/has_toolchanger.h>
#include <option/has_wastebin_fill_tracking.h>

using ScreenMenuFilament__ = ScreenMenu<GuiDefaults::MenuFooter,
    MI_RETURN,
#if HAS_WASTEBIN_FILL_TRACKING()
    MI_NOZZLE_CLEANER_EMPTY_WASTEBIN,
#endif
    MI_LOAD, MI_UNLOAD, MI_CHANGE,
    MI_PURGE,
#if HAS_TOOLCHANGER()
    MI_CHANGEALL,
#endif
    MI_FILAMENT_MANAGEMENT, //
    MI_TYPE_AND_COLOR //
    >;

class ScreenMenuFilament : public ScreenMenuFilament__ {
public:
    constexpr static const char *label = N_("FILAMENT");
    ScreenMenuFilament();
};
