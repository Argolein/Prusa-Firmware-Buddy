/**
 * @file MItem_advanced_settings.hpp
 */

#pragma once

#include "WindowMenuItems.hpp"

enum class AdvancedSettingsClickCommand {
    Reset_motor_currents,
    Reset_homing_sensitivity,
};

class MI_ADV_STEPS_PER_UNIT_X : public WiSpin {
public:
    MI_ADV_STEPS_PER_UNIT_X();

protected:
    void OnClick() override;
};

class MI_ADV_STEPS_PER_UNIT_Y : public WiSpin {
public:
    MI_ADV_STEPS_PER_UNIT_Y();

protected:
    void OnClick() override;
};

class MI_ADV_STEPS_PER_UNIT_Z : public WiSpin {
public:
    MI_ADV_STEPS_PER_UNIT_Z();

protected:
    void OnClick() override;
};

class MI_ADV_STEPS_PER_UNIT_E : public WiSpin {
public:
    MI_ADV_STEPS_PER_UNIT_E();

protected:
    void OnClick() override;
};

class MI_ADV_CURRENT_X : public WiSpin {
public:
    MI_ADV_CURRENT_X();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_CURRENT_Y : public WiSpin {
public:
    MI_ADV_CURRENT_Y();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_CURRENT_Z : public WiSpin {
public:
    MI_ADV_CURRENT_Z();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_CURRENT_E : public WiSpin {
public:
    MI_ADV_CURRENT_E();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_CURRENT_RESET_DEFAULTS : public IWindowMenuItem {
public:
    MI_ADV_CURRENT_RESET_DEFAULTS();

protected:
    void click(IWindowMenu &window_menu) override;
};

class MI_ADV_HOMING_SENS_X : public WiSpin {
public:
    MI_ADV_HOMING_SENS_X();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_HOMING_SENS_Y : public WiSpin {
public:
    MI_ADV_HOMING_SENS_Y();

    void Store();

protected:
    void OnClick() override;
};

class MI_ADV_HOMING_SENS_RESET_DEFAULTS : public IWindowMenuItem {
public:
    MI_ADV_HOMING_SENS_RESET_DEFAULTS();

protected:
    void click(IWindowMenu &window_menu) override;
};

class MI_ADV_PREHEAT_FOR_UNLOADING : public WI_ICON_SWITCH_OFF_ON_t {
    constexpr static const char *const label = "Preheat before unloading";

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
