#include "phase_stepping_mock.hpp"

#include <FreeRTOS.h>
#include <device/cmsis.h>
#include <feature/phase_stepping/phase_stepping.hpp>

uint32_t SystemCoreClock = 180000000;

size_t xPortGetFreeHeapSize(void) {
    return phase_stepping_mock::free_heap;
}

namespace phase_stepping_mock {

bool speed_model_enabled;
phase_stepping::diagnostics::IsrTiming isr;
phase_stepping::diagnostics::CalibrationHeap heap;
size_t free_heap;
std::array<uint32_t, 2> missed_tx;
std::array<phase_stepping::calibration_verify::MotorVerification, 2> records;
phase_stepping::calibration_check::CheckResult check;

void reset() {
    speed_model_enabled = false;
    isr = {};
    heap = {};
    free_heap = 0;
    missed_tx = {};
    records = {};
    check = {};
    phase_stepping::axis_states = {};
}

} // namespace phase_stepping_mock

namespace phase_stepping {

std::array<AxisState, opts::SUPPORTED_AXIS_COUNT> axis_states;

namespace diagnostics {

    IsrTiming isr_timing() {
        return phase_stepping_mock::isr;
    }

    CalibrationHeap last_calibration_heap() {
        return phase_stepping_mock::heap;
    }

    uint32_t missed_tx_total(AxisEnum axis) {
        return phase_stepping_mock::missed_tx[axis];
    }

    bool speed_model_enabled() {
        return phase_stepping_mock::speed_model_enabled;
    }

} // namespace diagnostics

namespace calibration_verify {

    const MotorVerification &record(AxisEnum axis) {
        return phase_stepping_mock::records[axis];
    }

} // namespace calibration_verify

namespace calibration_check {

    const CheckResult &last() {
        return phase_stepping_mock::check;
    }

} // namespace calibration_check

} // namespace phase_stepping
