#include "filament_command.h"
#include "handler.h"
#include "json_parser.h"

#include <config_store/store_instance.hpp>
#include <utils/color.hpp>
#include <tool_index.hpp>

#include <cstring>

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
    std::optional<uint8_t> new_color_index;
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
        } else if (key == "color") {
            if (event.type == Type::String) {
                const auto index = filament_color_palette_index_by_name(event.value.value());
                if (!index.has_value()) {
                    bad_value = true;
                } else {
                    new_color_index = index;
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

    // Apply the type first; setting it to none also clears the color, so the color
    // (if provided) is applied afterwards to take precedence.
    if (type_action == FieldAction::set) {
        config_store().set_filament_type(virtual_tool, new_type);
    } else if (type_action == FieldAction::clear) {
        config_store().set_filament_type(virtual_tool, FilamentType::none);
    }

    if (color_action == FieldAction::set) {
        config_store().set_filament_color(virtual_tool.to_physical(), new_color_index);
    } else if (color_action == FieldAction::clear) {
        config_store().set_filament_color(virtual_tool.to_physical(), std::nullopt);
    }

    return StatusPage(Status::NoContent, close, json_errors);
}

} // namespace nhttp::printer
