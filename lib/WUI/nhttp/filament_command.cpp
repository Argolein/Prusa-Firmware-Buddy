#include "filament_command.h"
#include "handler.h"
#include "json_parser.h"

#include "filament_renderer.h"

#include <config_store/store_instance.hpp>
#include <filament.hpp>
#include <tool_index.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <strings.h>

namespace nhttp::printer {

using namespace handler;
using http::Status;
using json::Event;
using json::Type;
using std::nullopt;
using std::string_view;

namespace {
    // Whether a JSON field was present and what to do with it.
    enum class FieldAction {
        absent,
        set,
        clear,
    };

    std::optional<Color> palette_color(string_view name) {
        for (const auto &preset : link_content::filament_color_palette) {
            if (name.size() == preset.name.size() && strncasecmp(name.data(), preset.name.data(), name.size()) == 0) {
                return preset.color;
            }
        }
        return nullopt;
    }

    /// Accepts exactly "#rrggbb".
    std::optional<Color> hex_color(string_view hex) {
        if (hex.size() != 7 || hex[0] != '#' || !std::all_of(hex.begin() + 1, hex.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })) {
            return nullopt;
        }
        return Color::from_string(hex);
    }
} // namespace

FilamentCommand::FilamentCommand(uint8_t tool, size_t content_length, bool can_keep_alive, bool json_errors)
    : tool(tool)
    , content_length(content_length)
    , can_keep_alive(can_keep_alive)
    , json_errors(json_errors) {
    memset(buffer.data(), 0, buffer.size());
}

void FilamentCommand::step(string_view input, bool terminated_by_client, uint8_t *, size_t, Step &out) {
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

StatusPage FilamentCommand::process() {
    const auto close = can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close;

    FieldAction type_action = FieldAction::absent;
    FieldAction color_action = FieldAction::absent;
    FilamentType new_type = FilamentType::none;
    OptionalColor new_color;
    bool bad_value = false;

    const auto is_null = [](const Event &event) {
        return event.type == Type::Primitive && event.value.has_value() && event.value.value() == "null";
    };

    const auto parse_result = parse_command(reinterpret_cast<char *>(buffer.data()), buffer_used, [&](const Event &event) {
        if (event.depth != 1 || !event.key.has_value()) {
            return;
        }
        const auto &key = event.key.value();

        if (key == "type") {
            if (event.type == Type::String) {
                const FilamentType ft = FilamentType::from_name(event.value.value());
                if (ft == FilamentType::none) {
                    bad_value = true;
                } else {
                    new_type = ft;
                    type_action = FieldAction::set;
                }
            } else if (is_null(event)) {
                type_action = FieldAction::clear;
            } else {
                bad_value = true;
            }
        } else if (key == "color" || key == "color_rgb") {
            if (event.type == Type::String) {
                const auto color = (key == "color") ? palette_color(event.value.value()) : hex_color(event.value.value());
                if (!color.has_value()) {
                    bad_value = true;
                } else {
                    new_color = *color;
                    color_action = FieldAction::set;
                }
            } else if (is_null(event)) {
                color_action = FieldAction::clear;
            } else {
                bad_value = true;
            }
        }
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
        return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Unknown filament type or color");
    }

    if (type_action == FieldAction::absent && color_action == FieldAction::absent) {
        return StatusPage(Status::BadRequest, close, json_errors, nullopt, "Nothing to set");
    }

    const auto virtual_tool = VirtualToolIndex::from_raw(tool);

    // Reject before writing anything, so a 409 never leaves a half-applied request behind.
    const bool type_after_none = (type_action == FieldAction::clear)
        || (type_action == FieldAction::absent && FilamentType::for_tool(virtual_tool) == FilamentType::none);
    if (color_action != FieldAction::absent && type_after_none) {
        return StatusPage(Status::Conflict, close, json_errors, nullopt, "No filament type set for this tool");
    }

    // Type first: a new type starts without a color, so a color sent along applies to it.
    if (type_action == FieldAction::set) {
        config_store().set_filament_type(virtual_tool, new_type);
    } else if (type_action == FieldAction::clear) {
        config_store().set_filament_type(virtual_tool, FilamentType::none);
    }

    if (color_action != FieldAction::absent) {
        const FilamentType loaded = FilamentType::for_tool(virtual_tool);

        // Only ad-hoc types carry a color. Like the printer's "Customize" action,
        // the loaded preset becomes this tool's ad-hoc copy.
        // Presets never carry a color, so clearing one is a no-op.
        if (color_action == FieldAction::set || loaded.is_adhoc()) {
            FilamentTypeParameters params = loaded.parameters();
            params.color = (color_action == FieldAction::set) ? new_color : OptionalColor {};
            const FilamentType adhoc = AdHocFilamentType { .tool = virtual_tool };
            adhoc.set_parameters(params);
            if (loaded != adhoc) {
                config_store().set_filament_type(virtual_tool, adhoc);
            }
        }
    }

    return StatusPage(Status::NoContent, close, json_errors);
}

} // namespace nhttp::printer
