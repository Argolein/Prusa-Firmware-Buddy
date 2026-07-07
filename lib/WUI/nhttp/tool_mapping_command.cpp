#include "tool_mapping_command.h"
#include "handler.h"
#include "json_parser.h"

#include <str_utils.hpp>

#include <option/has_tool_mapping.h>
#include <option/has_spool_join.h>

#if HAS_TOOL_MAPPING()
    #include <tool_index.hpp>
    #include <gcode/gcode_info.hpp>
    #include <marlin_vars.hpp>
    #include <fsm_states.hpp>
    #include <client_response.hpp>
    #include <fsm/print_preview_phases.hpp>
    #include <module/prusa/tool_mapper.hpp>
    #if HAS_SPOOL_JOIN()
        #include <module/prusa/spool_join.hpp>
    #endif
#endif

#include <cstring>

namespace nhttp::printer {

using namespace handler;
using http::Status;
using json::Event;
using json::Type;
using std::nullopt;
using std::string_view;

#if HAS_TOOL_MAPPING()
namespace {
    // True while the print-preview FSM is holding at the tools-mapping phase.
    // Mapping may only be edited then (otherwise we would scramble a running or
    // unrelated print's mapping).
    bool in_tools_mapping_phase() {
        std::optional<fsm::States::Top> top;
        marlin_vars().peek_fsm_states([&](const fsm::States &states) {
            top = states.get_top();
        });
        return top.has_value()
            && top->fsm_type == ClientFSM::PrintPreview
            && GetEnumFromPhaseIndex<PhasesPrintPreview>(top->data.GetPhase()) == PhasesPrintPreview::tools_mapping;
    }
} // namespace
#endif

ToolMappingCommand::ToolMappingCommand(size_t content_length, bool can_keep_alive, bool json_errors)
    : content_length(content_length)
    , can_keep_alive(can_keep_alive)
    , json_errors(json_errors) {
    memset(buffer.data(), 0, buffer.size());
}

void ToolMappingCommand::step(string_view input, bool terminated_by_client, uint8_t *, size_t, Step &out) {
    if (content_length > buffer.size()) {
        out = Step { 0, 0, StatusPage(Status::PayloadTooLarge, StatusPage::CloseHandling::ErrorClose, json_errors) };
        return;
    }

    const size_t rest = content_length - buffer_used;
    const size_t to_read = std::min(input.size(), rest);

    memcpy(buffer.data() + buffer_used, input.data(), to_read);
    buffer_used += to_read;

    if (content_length > buffer_used) {
        if (terminated_by_client) {
            out = Step { to_read, 0, StatusPage(Status::BadRequest, StatusPage::CloseHandling::ErrorClose, json_errors, nullopt, "Truncated request") };
        } else {
            out = Step { to_read, 0, Continue() };
        }
        return;
    }

    out = Step { to_read, 0, process() };
}

#if HAS_TOOL_MAPPING()
StatusPage ToolMappingCommand::process() {
    const auto close = can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close;

    if (!in_tools_mapping_phase()) {
        return StatusPage(Status::Conflict, close, json_errors, nullopt, "Not awaiting tool mapping");
    }

    // Positional arrays keyed by raw tool index; 0 means "none". Parsed with the
    // flat event callback: a named array switches the accumulation target and
    // resets the position; subsequent primitives fill it in order.
    std::array<uint8_t, GcodeToolIndex::count> mapping {};
    std::array<uint8_t, VirtualToolIndex::count> spool {};
    bool bad_value = false;

    enum class Mode {
        none,
        mapping,
        spool,
    };
    Mode mode = Mode::none;
    size_t pos = 0;

    const auto parse_result = parse_command(reinterpret_cast<char *>(buffer.data()), buffer_used, [&](const Event &event) {
        if (event.key.has_value()) {
            // A named field (depth 1) — decide what the following array holds.
            const auto &key = event.key.value();
            if (event.type == Type::Array && key == "mapping") {
                mode = Mode::mapping;
                pos = 0;
            } else if (event.type == Type::Array && key == "spool_join") {
                mode = Mode::spool;
                pos = 0;
            } else {
                mode = Mode::none;
            }
            return;
        }

        if (mode == Mode::none || event.type != Type::Primitive || !event.value.has_value()) {
            return;
        }

        const auto &val = event.value.value();
        uint8_t parsed = 0;
        if (from_chars_light(val.begin(), val.end(), parsed).ec != std::errc {}) {
            bad_value = true;
            return;
        }

        if (mode == Mode::mapping) {
            if (pos < mapping.size()) {
                mapping[pos] = parsed;
            }
        } else {
            if (pos < spool.size()) {
                spool[pos] = parsed;
            }
        }
        pos++;
    });

    switch (parse_result) {
    case JsonParseResult::ErrMem:
        return StatusPage(Status::PayloadTooLarge, close, json_errors, nullopt, "Too many JSON tokens");
    case JsonParseResult::ErrReq:
        return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Couldn't parse JSON");
    case JsonParseResult::Ok:
        break;
    }

    if (bad_value) {
        return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Invalid tool index");
    }

    const auto cleanup = []() {
        tool_mapper.reset();
    #if HAS_SPOOL_JOIN()
        spool_join.reset();
    #endif
        tool_mapper.set_enable(false);
    };

    // Replace the whole mapping (don't merge with the default 1-1 mapping).
    tool_mapper.set_all_unassigned();
    tool_mapper.set_enable(true);
    #if HAS_SPOOL_JOIN()
    spool_join.reset();
    #endif

    for (uint8_t g = 0; g < GcodeToolIndex::count; g++) {
        const uint8_t disp = mapping[g];
        if (disp == 0) {
            continue; // leave unassigned
        }
        if (disp > VirtualToolIndex::count) {
            cleanup();
            return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Tool out of range");
        }
        if (!tool_mapper.set_mapping(GcodeToolIndex::from_raw(g), VirtualToolIndex::from_raw(disp - 1))) {
            cleanup();
            return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Invalid tool mapping");
        }
    }

    #if HAS_SPOOL_JOIN()
    for (uint8_t v = 0; v < VirtualToolIndex::count; v++) {
        const uint8_t disp = spool[v];
        if (disp == 0) {
            continue;
        }
        if (disp > VirtualToolIndex::count) {
            cleanup();
            return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Tool out of range");
        }
        if (!spool_join.add_join(VirtualToolIndex::from_raw(v), VirtualToolIndex::from_raw(disp - 1))) {
            cleanup();
            return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Invalid spool join");
        }
    }
    #endif

    // Every filament the G-code actually uses must end up mapped: the tool mapper
    // is a bijection (mapping a second filament onto a tool un-maps the first), so
    // an accidental collision would otherwise fatal-error at print time (T.cpp).
    const auto &gci = GCodeInfo::getInstance();
    for (const auto gcode_tool : GcodeToolIndex::all()) {
        if (!gci.get_extruder_info(gcode_tool).used()) {
            continue;
        }
        if (!std::holds_alternative<VirtualToolIndex>(tool_mapper.to_virtual(gcode_tool))) {
            cleanup();
            return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Every filament must be mapped to a distinct tool");
        }
    }

    return StatusPage(Status::NoContent, close, json_errors);
}
#else
StatusPage ToolMappingCommand::process() {
    const auto close = can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close;
    return StatusPage(Status::NotFound, close, json_errors);
}
#endif

} // namespace nhttp::printer
