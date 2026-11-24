#pragma once

#include "speed_model.hpp"

#include <array>
#include <optional>
#include <span>

/// Calibration of the speed models: the maths of the phase-versus-speed sweep
/// (INDX only). Free of firmware headers so the host tests build it.
namespace phase_stepping::calibration_speed {

inline constexpr int SPEED_COUNT = 3;
/// Each speed is measured this many times; repeats are adjacent in the inputs.
inline constexpr int REPEATS = 2;
inline constexpr float ALIAS_MARGIN_HZ = 30.f;
inline constexpr float SPEED_STEP = 0.01f;
inline constexpr int MAX_SPEED_STEPS = 20;
inline constexpr int MAX_ALIAS_HARMONIC = 16;
/// Shortest constant-speed part of a sweep move: 20 analysis windows of 10
/// electrical periods, i.e. 10 windows per turn of the two-turn phase sweep.
inline constexpr float MIN_SWEEP_REVS = 2.f;
/// Room for the marker and no-movement blocks of a capture.
inline constexpr float MOVE_MARGIN_REVS = 0.05f;

/// Sweep speeds of a harmonic, rev/s.
struct HarmonicPlan {
    /// Top of the band the accelerometer observes for this harmonic.
    float band_limit;
    std::array<float, SPEED_COUNT> default_speeds;
    /// Measure the size at the top speed and fit a size slope.
    bool fit_size;
};

/// Plan for @p harmonic, none for harmonics without a speed model.
std::optional<HarmonicPlan> plan_for(int harmonic);

/// Frequency @p freq folded into [0, sampling_freq / 2].
float alias(float freq, float sampling_freq);

/// No harmonic m <= MAX_ALIAS_HARMONIC other than @p harmonic folds within
/// ALIAS_MARGIN_HZ of harmonic @p harmonic at motor speed @p speed.
bool is_alias_free(float speed, int harmonic, float sampling_freq, int periods_per_rev);

/// The speed closest to @p target (steps of SPEED_STEP, at most
/// MAX_SPEED_STEPS away, within (0, @p max_speed]) that is alias free; NaN if
/// there is none.
float select_speed(float target, int harmonic, float sampling_freq, int periods_per_rev, float max_speed);

/// Duration of the constant-speed part of a capture move at @p speed:
/// @p duration, shortened so the whole move (the part plus the settle runs of
/// @p settle_time on both sides, the ramps at @p accel and MOVE_MARGIN_REVS)
/// stays within @p max_revs. NaN when shortening leaves less than MIN_SWEEP_REVS.
float capped_sweep_duration(float speed, float duration, float max_revs, float settle_time, float accel);

/// Total revolutions of a capture move with a constant-speed part of @p duration.
float capture_move_revs(float speed, float duration, float settle_time, float accel);

/// @p phase moved by whole turns to lie within π of @p reference.
float unwrap_near(float phase, float reference);

/// Least squares fit of phase(v) = p0 - slope * (v - v0).
struct PhaseFit {
    float p0;
    float slope;
    /// NaN with fewer than three points.
    float slope_se;
};
PhaseFit fit_phase(std::span<const float> speeds, std::span<const float> phases, float v0);

/// Least squares slope g of size(v) = size0 * (1 + g * (v - v0)) with size0 fixed.
float fit_size_slope(std::span<const float> speeds, std::span<const float> sizes, float v0, float size0);

/// @p size_slope limited so the size factor stays within
/// [MIN_SIZE_FACTOR, MAX_SIZE_FACTOR] between @p bottom and @p top.
float clamp_size_slope(float size_slope, float v0, float bottom, float top);

/// The phase change across the band exceeds twice the repeat spread.
bool slope_exceeds_spread(float slope, float bottom, float top, float spread);

/// Fitted sweep of one harmonic and direction. `model.active` is not set here.
struct SweepFit {
    speed_model::HarmonicModel model;
    /// Unwrapped phases, same order as the input.
    std::array<float, SPEED_COUNT * REPEATS> phases;
    float band_bottom;
    /// Largest difference between repeats of one speed.
    float spread;
    bool significant;
};

/// Fit the phases measured at @p speeds (ascending, REPEATS adjacent
/// measurements per speed, at most SPEED_COUNT speeds). The first phase is
/// unwrapped near @p reference_phase, every later one near its predecessor.
SweepFit fit_sweep(std::span<const float> speeds, std::span<const float> phases, float v0, float reference_phase);

} // namespace phase_stepping::calibration_speed
