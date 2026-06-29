/**
 * @file indx_speed_led.cpp
 * @see indx_speed_led.hpp
 */
#include "indx_speed_led.hpp"

#include <option/has_indx.h>

#if HAS_INDX()
    #include <cstdint>

    #include <config_store/store_instance.hpp>
    #include <indx_head/leds.hpp>
    #include <puppies/INDX.hpp>

    #include "../Marlin/src/module/planner.h"
#endif

namespace indx_speed_led {

#if HAS_INDX()
namespace {

    /// Cruise speed (mm/s) of the move currently being executed, 0 when idle.
    ///
    /// Plain cross-thread read of the planner block buffer: the executing block
    /// may be discarded by the stepper ISR right after we snapshot the tail, but
    /// a stale/torn nominal_speed only costs one slightly-off sample, which the
    /// EWMA below smooths away. nominal_speed is constant within a block, so the
    /// resulting color is stable mid-move.
    float current_head_speed_mm_s() {
        if (!planner.has_blocks_queued()) {
            return 0.f;
        }
        const uint8_t tail = planner.block_buffer_tail;
        block_t &block = planner.block_buffer[tail];
        return block.is_move() ? block.nominal_speed : 0.f;
    }

    /// Exponential smoothing factor (per ~100 ms tick) so the color glides across
    /// move boundaries instead of snapping.
    constexpr float smoothing = 0.25f;

    float ewma_speed_mm_s = 0.f;
    Color last_pushed_color = COLOR_BLACK;
    bool have_pushed = false;

} // namespace
#endif

void update() {
#if HAS_INDX()
    if (!config_store().tool_leds_enabled.get() || !config_store().tool_leds_speed_reactive.get()) {
        // Disabled: forget the last color so re-enabling forces a fresh push.
        have_pushed = false;
        return;
    }

    const float speed = current_head_speed_mm_s();
    ewma_speed_mm_s += (speed - ewma_speed_mm_s) * smoothing;

    const Color color = speed_to_color(static_cast<int>(ewma_speed_mm_s + 0.5f));

    // Only flush when the color actually changes -> no Modbus traffic at a
    // constant speed or when idle.
    if (!have_pushed || color != last_pushed_color) {
        last_pushed_color = color;
        have_pushed = true;
        buddy::puppies::indx.set_leds_color(color, indx_head::leds::Mode::solid);
    }
#endif
}

} // namespace indx_speed_led
