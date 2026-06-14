#pragma once

#include <cstdint>
#include <module/planner.h>

namespace pressure_advance {

struct Config {
    float pressure_advance = 0.f;
    float smooth_time = 0.04f;

    bool operator==(const Config &rhs) const = default;
};

// pressure advance initialization
void init();

// configure e axis
void set_axis_e_config(const Config &config);
const Config &get_axis_e_config();

// Per-move "queued" PA value used by the hot path of M572 S<x>: the gcode-thread
// snapshot that gets copied into each new block_t at planner.buffer_segment time
// and propagated into move_t segments. Updating this does not flush the planner.
float get_queued_value();
void set_queued_value(float v);

// True iff a runtime M572 S<x> can take the hot (queued, no-flush) path:
// - the PA step generator is already armed,
// - the new PA value is > 0 (M572 S0 is a documented disable and must go
//   through the structural path so the generator bit and FIR lookback are
//   actually cleared — homing / probe rely on this via PressureAdvanceDisabler),
// - the requested smooth_time matches the currently active smooth_time,
// - PressureAdvanceDisabler is not currently active.
// Caller (M572_internal) falls back to the structural flush+set_axis_e_config
// path when this returns false.
bool can_use_queued_path(const Config &new_cfg);

// Update the value reported by M572 (no args) to match the queued PA value.
// Does not touch the FIR filter or step-generator state.
void update_reported_value(float v);

// Guard to globally disable PA
class PressureAdvanceDisabler {
    static inline unsigned nesting = 0;

    // Original PA settings
    static inline Config config_orig;

public:
    [[nodiscard]] PressureAdvanceDisabler() {
        if (nesting == 0) {
            config_orig = get_axis_e_config();
            if (config_orig.pressure_advance > 0.f) {
                planner.synchronize();
                set_axis_e_config({ .pressure_advance = 0.f });
            }
        }
        ++nesting;
    }

    ~PressureAdvanceDisabler() {
        if (--nesting == 0) {
            if (config_orig != get_axis_e_config()) {
                planner.synchronize();
                set_axis_e_config(config_orig);
            }
        }
    }

    static bool is_active() {
        return nesting > 0;
    }
};

} // namespace pressure_advance
