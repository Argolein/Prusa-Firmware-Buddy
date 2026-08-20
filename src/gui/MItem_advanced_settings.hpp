/**
 * @file MItem_advanced_settings.hpp
 */

#pragma once

#include "WindowMenuItems.hpp"

class MI_ADV_PREHEAT_FOR_UNLOADING : public WI_ICON_SWITCH_OFF_ON_t {
    // ON (default): Prusa behaviour — preheat the nozzle and ram before unloading.
    // OFF: cold unload — skip the preheat AND skip ramming (ramming cold risks a jam).
    constexpr static const char *const label = "Preheat & ram before unload";

public:
    MI_ADV_PREHEAT_FOR_UNLOADING();
    virtual void OnChange(size_t old_index) override;
};

class MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE : public WI_ICON_SWITCH_OFF_ON_t {
    constexpr static const char *const label = "Cooldown nozzle after loading when idle";

public:
    MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE();
    virtual void OnChange(size_t old_index) override;
};

class MI_ADV_AUTO_Z_ALIGN_HOMING : public WI_ICON_SWITCH_OFF_ON_t {
    constexpr static const char *const label = "Auto Z align when homing";

public:
    MI_ADV_AUTO_Z_ALIGN_HOMING();
    virtual void OnChange(size_t old_index) override;
};
