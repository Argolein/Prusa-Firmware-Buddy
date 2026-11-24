#pragma once

#include <buddy/phase_stepping_opts.h>
#include <diagnostics.hpp>
#include <lut.hpp>

#include <array>
#include <cstdio>
#include <string>

namespace phase_stepping {

/// The part of the firmware's axis state the speed model runtime touches.
struct AxisState {
    CorrectedCurrentLut forward_current, backward_current;
};

extern std::array<AxisState, opts::SUPPORTED_AXIS_COUNT> axis_states;

} // namespace phase_stepping

/// Replacements the firmware sources under test are compiled against.
namespace test_seams {

/// Host directory that stands in for `/system`.
const std::string &system_dir();
/// Host path of a firmware path; `/system/...` maps into system_dir().
std::string host_path(const char *path);

/// Returned by `diagnostics::last_calibration_heap`, set by `restore_calibration_heap`.
inline phase_stepping::diagnostics::CalibrationHeap calibration_heap {};
/// Returned by `diagnostics::speed_model_enabled`.
inline bool speed_model_enabled = true;

/// `malloc_fallible` calls so far, including failed ones.
inline int allocation_calls = 0;
/// Successful `malloc_fallible` calls.
inline int allocations = 0;
/// Pointers freed through test_free.
inline int frees = 0;
/// 1-based `malloc_fallible` call that returns null; 0 = none.
inline int failing_call = 0;

inline void reset_allocations() {
    allocation_calls = allocations = frees = failing_call = 0;
}

} // namespace test_seams

FILE *test_fopen(const char *path, const char *mode);
int test_unlink(const char *path);
void test_free(void *ptr);
