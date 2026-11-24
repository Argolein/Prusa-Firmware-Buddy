#pragma once

// Stand-in for the real phase stepping header (which pulls in Marlin): only the
// per-axis state the PrusaLink renderer reads.

#include <feature/phase_stepping/lut.hpp>

#include <array>

namespace phase_stepping {

struct AxisState {
    bool enabled = false;
    CorrectedCurrentLut forward_current, backward_current;
};

extern std::array<AxisState, opts::SUPPORTED_AXIS_COUNT> axis_states;

} // namespace phase_stepping
