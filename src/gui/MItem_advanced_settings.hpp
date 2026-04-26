/**
 * @file MItem_advanced_settings.hpp
 */

#pragma once

#include "WindowMenuItems.hpp"

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
