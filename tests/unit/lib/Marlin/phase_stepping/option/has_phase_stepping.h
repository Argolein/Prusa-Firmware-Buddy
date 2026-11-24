#pragma once
// The firmware header with phase stepping on pulls in the motion stack; the
// tests only need the LUTs, so they take the dummies branch and declare the
// axis states themselves.
#define HAS_PHASE_STEPPING() 0

namespace option {

static constexpr bool has_phase_stepping = false;

}
