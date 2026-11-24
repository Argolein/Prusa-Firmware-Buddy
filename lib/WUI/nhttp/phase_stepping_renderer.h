#pragma once

#include "segmented_json.h"

#include <feature/phase_stepping/common.hpp>

#include <array>
#include <cstdint>

namespace nhttp::link_content {

/// Streams phase stepping diagnostics and the corrections in use for
/// GET /api/v1/phase-stepping (INDX only).
///
/// Scalar diagnostics are snapshotted at construction. The correction
/// spectra are copied one at a time, right before each is rendered, so the
/// connection state holds a single spectrum. Each copy is taken once per
/// request; a calibration writing the table at the same moment may leave the
/// copy mixing old and new harmonics. Non-finite floats render as `null`.
class PhaseSteppingRenderState {
public:
    PhaseSteppingRenderState();

    bool speed_model_enabled;
    /// Highest version of the calibration records in RAM, 0 = none.
    uint16_t calibration_version;
    uint32_t isr_max_cycles;
    uint32_t isr_mean_cycles;
    uint32_t isr_samples;
    uint32_t cpu_hz;
    bool heap_valid;
    uint32_t heap_min_free_bytes;
    uint32_t heap_free_chunks_at_min;

    uint8_t motor = 0; ///< 0 = X / A, 1 = Y / B
    uint8_t direction = 0; ///< 0 = forward, 1 = backward
    uint8_t harmonic = 0;
    bool motor_enabled = false;
    uint32_t motor_missed_tx = 0;
    phase_stepping::MotorPhaseCorrection spectrum {};
    std::array<char, 40> pair {}; ///< formatted current `[mag,pha]`
    std::array<char, 96> verification {}; ///< formatted current verification entry
    std::array<char, 192> speed_model {}; ///< formatted current speed model entry
    bool verification_emitted = false; ///< an entry of the current array is out

    /// Reads the current motor's enabled flag and missed TX count.
    void load_motor();
    /// Copies the current motor's spectrum for the current direction.
    void load_spectrum();
    /// Formats the current harmonic of `spectrum` into `pair`.
    void load_pair();
    /// Formats the verification of the current motor, direction and harmonic
    /// into `verification`. False when that harmonic was not calibrated.
    bool load_verification();
    /// Formats the speed model of the current motor, direction and harmonic
    /// into `speed_model`. False when that harmonic had no speed sweep.
    bool load_speed_model();
};

class PhaseSteppingRenderer final : public json::JsonRenderer<PhaseSteppingRenderState> {
public:
    PhaseSteppingRenderer()
        : JsonRenderer(PhaseSteppingRenderState()) {}
    json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, PhaseSteppingRenderState &state) const override;
};

} // namespace nhttp::link_content
