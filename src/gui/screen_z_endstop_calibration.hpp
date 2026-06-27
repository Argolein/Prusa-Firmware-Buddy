#pragma once

#include "screen_fsm.hpp"
#include "radio_button_fsm.hpp"

class ScreenZEndstopCalibration final : public ScreenFSM {
private:
    RadioButtonFSM radio;

public:
    ScreenZEndstopCalibration();
    ~ScreenZEndstopCalibration();

protected:
    void create_frame() final;
    void destroy_frame() final;
    void update_frame() final;
};
