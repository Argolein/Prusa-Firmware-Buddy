#pragma once

#include "status_page.h"

#include <http/types.h>

#include <array>
#include <cstdint>
#include <string_view>

namespace nhttp::printer {

/**
 * \brief Handler for PUT /api/v1/filament/<tool>.
 *
 * Accepts a small JSON body { "type": "PLA"|null, "color": "Red"|null, "color_rgb": "#rrggbb"|null }.
 * "color" is a palette name (case-insensitive), "color_rgb" any RGB value. A color is stored
 * on the tool's ad-hoc filament (Prusa's per-load copy), so it needs a filament type and is
 * lost on unload. 409 when a color is sent for a tool without a filament type.
 */
class FilamentCommand {
private:
    // The JSON parser wants the whole body in one block; gather it here (same approach
    // as JobCommand). The body is tiny, so this small buffer is plenty.
    static const constexpr size_t BUFFER_LEN = 100;
    std::array<uint8_t, BUFFER_LEN> buffer;
    size_t buffer_used = 0;
    uint8_t tool;
    size_t content_length;
    bool can_keep_alive;
    bool json_errors;

    handler::StatusPage process();

public:
    FilamentCommand(uint8_t tool, size_t content_length, bool can_keep_alive, bool json_errors);
    bool want_read() const { return true; }
    bool want_write() const { return false; }
    void step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size, handler::Step &out);
};

} // namespace nhttp::printer
