#include "gcode_command.h"
#include "gcode_reply.h"
#include "handler.h"
#include "json_parser.h"
#include "marlin_client.hpp"
#include "config.h"
#include <marlin_server.hpp>

#include <cassert>
#include <cstring>

namespace nhttp::printer {

using namespace handler;
using http::Status;
using json::Event;
using json::Type;
using std::nullopt;
using std::string_view;

namespace {

    ConnectionState process_gcode_command(char *request_buffer, size_t request_size, bool can_keep_alive, bool json_errors) {
        char gcode_buffer[MARLIN_MAX_REQUEST + 1] = { 0 };
        bool found_command = false;
        bool command_too_long = false;

        const auto parse_result = parse_command(request_buffer, request_size, [&](const Event &event) {
            if (event.depth != 1 || event.type != Type::String) {
                return;
            }
            if (event.key == "command") {
                found_command = true;
                const auto value = event.value.value();
                command_too_long = value.size() > MARLIN_MAX_REQUEST;
                if (!command_too_long) {
                    memcpy(gcode_buffer, value.data(), value.size());
                    gcode_buffer[value.size()] = '\0';
                }
            }
        });

        switch (parse_result) {
        case JsonParseResult::ErrMem:
            return StatusPage(Status::PayloadTooLarge, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Too many JSON tokens");
        case JsonParseResult::ErrReq:
            return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Couldn't parse JSON");
        case JsonParseResult::Ok:
            break;
        }

        if (!found_command) {
            return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Missing command field");
        }
        if (command_too_long) {
            return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "G-code command too long");
        }

        uint32_t response_id = 0;
        switch (marlin_server::start_gcode_response_capture(response_id)) {
        case marlin_server::GcodeResponseCaptureStartResult::Started:
            break;
        case marlin_server::GcodeResponseCaptureStartResult::Busy:
            return StatusPage(Status::Conflict, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Another G-code response is still pending");
        case marlin_server::GcodeResponseCaptureStartResult::Unsupported:
            return StatusPage(Status::NotImplemented, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "G-code response capture is not supported on this printer");
        }

        marlin_client::init_maybe();
        switch (marlin_client::gcode_try(gcode_buffer)) {
        case marlin_client::GcodeTryResult::Submitted:
            return GcodeReply::submitted(response_id, can_keep_alive);
        case marlin_client::GcodeTryResult::QueueFull:
            marlin_server::cancel_gcode_response_capture(response_id);
            return StatusPage(Status::Conflict, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "G-code queue is full");
        case marlin_client::GcodeTryResult::GcodeTooLong:
            marlin_server::cancel_gcode_response_capture(response_id);
            return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "G-code command too long");
        default:
            marlin_server::cancel_gcode_response_capture(response_id);
            return StatusPage(Status::InternalServerError, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors);
        }
    }

} // namespace

GcodeCommand::GcodeCommand(size_t content_length, bool can_keep_alive, bool json_errors)
    : content_length(content_length)
    , can_keep_alive(can_keep_alive)
    , json_errors(json_errors) {
    memset(buffer.data(), 0, buffer.size());
}

void GcodeCommand::step(std::string_view input, bool terminated_by_client, uint8_t *, size_t, Step &out) {
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

    out = Step { to_read, 0, process_gcode_command(reinterpret_cast<char *>(buffer.data()), buffer_used, can_keep_alive, json_errors) };
}

} // namespace nhttp::printer
