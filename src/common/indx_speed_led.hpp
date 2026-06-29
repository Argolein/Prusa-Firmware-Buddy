/**
 * @file indx_speed_led.hpp
 *
 * "Speed reactive" mode for the INDX tool-board RGB status LED (LP5817).
 *
 * Maps the current motion speed to a color (flat green up to ~90 mm/s, then a
 * gradient through yellow/orange to bright red at 300 mm/s) and pushes it to the
 * head via buddy::puppies::indx.set_leds_color().
 *
 * The mapping (speed_to_color) is a constexpr piecewise-linear interpolation and
 * is intentionally free of any dependency on motion/puppy code so it can be unit
 * tested in isolation. The glue (update()) lives in the .cpp and is guarded by
 * HAS_INDX().
 *
 * See docs/planning/indx-speed-reactive-led.md for the design rationale,
 * especially the "no measurable CPU load" constraint.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <utils/color.hpp>

namespace indx_speed_led {

/// One keyframe of the speed -> color ramp.
struct ColorStop {
    int16_t mm_s;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/// Speed -> color keyframes. Flat green up to 90 mm/s, then a gradient to a
/// bright red at 300 mm/s. These are sRGB targets; the head re-applies gamma 2.2
/// (see Indx::set_leds_color), so the physical LED looks a touch more saturated.
inline constexpr std::array<ColorStop, 9> color_stops { {
    { 0, 21, 194, 75 }, // green (idle / slow)
    { 90, 21, 194, 75 }, // green plateau ends
    { 120, 154, 214, 28 }, // lime
    { 150, 232, 208, 0 }, // yellow
    { 180, 255, 179, 0 }, // amber
    { 210, 255, 122, 0 }, // orange
    { 240, 255, 69, 0 }, // orange-red
    { 270, 240, 24, 0 }, // red
    { 300, 212, 0, 0 }, // bright red (fast)
} };

/// Integer linear interpolation of one channel between two stops.
/// \p t is in [0, \p span], \p span > 0.
constexpr uint8_t lerp_channel(uint8_t from, uint8_t to, int t, int span) {
    return static_cast<uint8_t>(static_cast<int>(from) + (static_cast<int>(to) - static_cast<int>(from)) * t / span);
}

/// Maps a speed in mm/s to the LED color, clamping below the first / above the
/// last keyframe. constexpr so the whole ramp is resolved at compile time.
constexpr Color speed_to_color(int speed_mm_s) {
    if (speed_mm_s <= color_stops.front().mm_s) {
        return Color::from_rgb(color_stops.front().r, color_stops.front().g, color_stops.front().b);
    }
    if (speed_mm_s >= color_stops.back().mm_s) {
        return Color::from_rgb(color_stops.back().r, color_stops.back().g, color_stops.back().b);
    }
    for (std::size_t i = 1; i < color_stops.size(); ++i) {
        if (speed_mm_s <= color_stops[i].mm_s) {
            const ColorStop &lo = color_stops[i - 1];
            const ColorStop &hi = color_stops[i];
            const int span = hi.mm_s - lo.mm_s;
            const int t = speed_mm_s - lo.mm_s;
            return Color::from_rgb(
                lerp_channel(lo.r, hi.r, t, span),
                lerp_channel(lo.g, hi.g, t, span),
                lerp_channel(lo.b, hi.b, t, span));
        }
    }
    return Color::from_rgb(color_stops.back().r, color_stops.back().g, color_stops.back().b);
}

/// Recompute the speed-reactive LED color and push it to the head if it changed.
///
/// Called periodically (~10 Hz) from the Marlin server loop. No-op (after two
/// cached config reads) unless both the Tool LEDs and the "Speed Reactive Light"
/// settings are enabled. Pushes a color only when it actually changes, so a
/// constant speed or an idle printer produces no Modbus traffic.
void update();

} // namespace indx_speed_led
