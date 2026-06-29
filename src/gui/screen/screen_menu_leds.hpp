/**
 * @file screen_menu_leds.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"
#include <option/has_toolchanger.h>
#include <option/has_indx.h>

using ScreenMenuLeds__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
#if HAS_LEDS()
    MI_LEDS_ENABLE,
#endif
#if HAS_TOOLCHANGER()
    MI_TOOL_LEDS_ENABLE,
#endif
#if HAS_INDX()
    MI_INDX_SPEED_LED,
#endif
#if HAS_SIDE_LEDS()
    MI_SIDE_LEDS_MAX_BRIGTHNESS,
    MI_SIDE_LEDS_DIMMING_ENABLE,
    MI_SIDE_LEDS_DIMMED_BRIGTHNESS,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLeds : public ScreenMenuLeds__ {
public:
    ScreenMenuLeds();
};
