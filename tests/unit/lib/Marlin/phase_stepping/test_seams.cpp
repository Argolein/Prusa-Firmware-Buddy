#include "test_seams.hpp"
#include "heap.h"

#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <unistd.h>

namespace phase_stepping {

std::array<AxisState, opts::SUPPORTED_AXIS_COUNT> axis_states {};

namespace diagnostics {

    CalibrationHeap last_calibration_heap() {
        return test_seams::calibration_heap;
    }

    void restore_calibration_heap(const CalibrationHeap &heap) {
        test_seams::calibration_heap = heap;
    }

    bool speed_model_enabled() {
        return test_seams::speed_model_enabled;
    }

} // namespace diagnostics

} // namespace phase_stepping

namespace test_seams {

const std::string &system_dir() {
    static const std::string dir = [] {
        // Static, so the exit handler never reads a string already destroyed.
        static char name[] = "/tmp/phase_stepping_tests_XXXXXX";
        if (!mkdtemp(name)) {
            std::abort();
        }
        std::atexit([] { std::filesystem::remove_all(name); });
        return std::string(name);
    }();
    return dir;
}

std::string host_path(const char *path) {
    constexpr std::string_view prefix = "/system/";
    const std::string_view view(path);
    if (!view.starts_with(prefix)) {
        return std::string(view);
    }
    return system_dir() + "/" + std::string(view.substr(prefix.size()));
}

} // namespace test_seams

FILE *test_fopen(const char *path, const char *mode) {
    return fopen(test_seams::host_path(path).c_str(), mode);
}

int test_unlink(const char *path) {
    return unlink(test_seams::host_path(path).c_str());
}

void test_free(void *ptr) {
    if (ptr) {
        test_seams::frees++;
    }
    std::free(ptr);
}

extern "C" void *malloc_fallible(size_t size) {
    if (++test_seams::allocation_calls == test_seams::failing_call) {
        return nullptr;
    }
    void *memory = std::malloc(size);
    if (memory) {
        test_seams::allocations++;
    }
    return memory;
}
