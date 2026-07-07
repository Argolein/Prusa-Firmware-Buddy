#pragma once

#include "status_page.h"

#include <http/types.h>

#include <array>
#include <cstdint>
#include <string_view>

namespace nhttp::printer {

/**
 * \brief Handler for PUT /api/v1/mapping (Argo: web tool mapping).
 *
 * Accepts a compact JSON body describing the G-code-filament → printer-tool
 * mapping and applies it to the live tool mapper (the same one the LCD tools
 * mapping screen drives). Only accepted while the print-preview FSM is holding
 * at the tools-mapping phase.
 *
 * Body (positional, 1-based tool display indices; 0 = none):
 * \code
 *   { "mapping":    [1, 1, 2],       // mapping[gcode_raw] = printer tool, 0 = unassigned
 *     "spool_join": [0, 3, 0] }      // spool_join[tool_raw] = tool to continue on, 0 = none
 * \endcode
 */
class ToolMappingCommand {
private:
    // The JSON parser wants the whole body in one block (same approach as
    // FilamentCommand). The body is a couple of short integer arrays.
    static const constexpr size_t BUFFER_LEN = 256;
    std::array<uint8_t, BUFFER_LEN> buffer;
    size_t buffer_used = 0;
    size_t content_length;
    bool can_keep_alive;
    bool json_errors;

    handler::StatusPage process();

public:
    ToolMappingCommand(size_t content_length, bool can_keep_alive, bool json_errors);
    bool want_read() const { return true; }
    bool want_write() const { return false; }
    void step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size, handler::Step &out);
};

} // namespace nhttp::printer
