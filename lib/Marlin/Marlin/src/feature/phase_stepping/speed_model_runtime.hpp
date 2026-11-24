#pragma once

#include <core/types.h>

/// Owner of the speed model tables in use (INDX only).
///
/// The tables are allocated on the heap (fallibly) only while a model is in
/// use, so a calibration, which runs with all models off, has the stock heap.
/// The ISR reaches a model through `CorrectedCurrentLut::_speed_model`.
namespace phase_stepping::speed_model_runtime {

/// Put the models of the calibration record of @p axis into use when the
/// speed model is enabled (`M975`), built from the corrections in RAM;
/// otherwise run the stock summed tables. A harmonic whose table cannot be
/// allocated stays stock. Call outside the ISR after the record or the
/// corrections changed.
void apply(AxisEnum axis);

/// Stop using any model of @p axis and restore the full summed tables.
void deactivate(AxisEnum axis);

} // namespace phase_stepping::speed_model_runtime
