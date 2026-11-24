#pragma once

#include <feature/phase_stepping/calibration_check.hpp>
#include <feature/phase_stepping/calibration_verify.hpp>
#include <feature/phase_stepping/diagnostics.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

/// Data the phase stepping accessors of phase_stepping_mock.cpp return.
/// Tests write it directly; the renderer snapshots part of it at construction.
namespace phase_stepping_mock {

extern bool speed_model_enabled;
extern phase_stepping::diagnostics::IsrTiming isr;
extern phase_stepping::diagnostics::CalibrationHeap heap;
extern size_t free_heap;
extern std::array<uint32_t, 2> missed_tx;
extern std::array<phase_stepping::calibration_verify::MotorVerification, 2> records;
extern phase_stepping::calibration_check::CheckResult check;

/// Back to the boot state: nothing calibrated, checked or measured.
void reset();

} // namespace phase_stepping_mock
