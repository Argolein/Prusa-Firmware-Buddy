#include "gcode_reply.h"
#include "handler.h"
#include "headers.h"
#include "status_page.h"

#include <algorithm>
#include <charconv>
#include <cstring>

namespace nhttp::printer {

using handler::Continue;
using handler::StatusPage;
using handler::Step;
using handler::Terminating;
using http::ConnectionHandling;
using http::ContentType;
using http::Status;
using std::string_view;

GcodeReply::GcodeReply(Status status, bool can_keep_alive)
    : status(status)
    , can_keep_alive(can_keep_alive) {
}

bool GcodeReply::append_raw(string_view text) {
    if (body_size + text.size() > body.size()) {
        return false;
    }
    memcpy(body.data() + body_size, text.data(), text.size());
    body_size += text.size();
    return true;
}

bool GcodeReply::append_bool(bool value) {
    return append_raw(value ? "true" : "false");
}

bool GcodeReply::append_uint(uint32_t value) {
    char number[16];
    const auto result = std::to_chars(number, number + sizeof(number), value);
    if (result.ec != std::errc {}) {
        return false;
    }
    return append_raw(string_view(number, result.ptr - number));
}

bool GcodeReply::append_json_string(string_view text) {
    if (!append_raw("\"")) {
        return false;
    }

    for (const char ch : text) {
        switch (ch) {
        case '\\':
        case '"':
            if (!append_raw("\\") || !append_raw(string_view(&ch, 1))) {
                return false;
            }
            break;
        case '\n':
            if (!append_raw("\\n")) {
                return false;
            }
            break;
        case '\r':
            if (!append_raw("\\r")) {
                return false;
            }
            break;
        case '\t':
            if (!append_raw("\\t")) {
                return false;
            }
            break;
        default:
            if (!append_raw(string_view(&ch, 1))) {
                return false;
            }
            break;
        }
    }

    return append_raw("\"");
}

GcodeReply GcodeReply::submitted(uint32_t id, bool can_keep_alive) {
    GcodeReply reply(Status::Ok, can_keep_alive);
    reply.append_raw("{\"id\":");
    reply.append_uint(id);
    reply.append_raw(",\"state\":\"pending\"}");
    return reply;
}

GcodeReply GcodeReply::from_capture(uint32_t id, bool can_keep_alive, bool json_errors) {
    marlin_server::GcodeResponseSnapshot snapshot;
    if (!marlin_server::get_gcode_response_capture(id, snapshot)) {
        return GcodeReply(Status::NotFound, can_keep_alive);
    }

    GcodeReply reply(Status::Ok, can_keep_alive);
    const char *state = snapshot.completed ? "complete" : "pending";
    const string_view response(snapshot.response.data(), strnlen(snapshot.response.data(), snapshot.response.size()));

    const bool body_ok
        = reply.append_raw("{\"id\":")
        && reply.append_uint(snapshot.id)
        && reply.append_raw(",\"state\":\"")
        && reply.append_raw(state)
        && reply.append_raw("\",\"completed\":")
        && reply.append_bool(snapshot.completed)
        && reply.append_raw(",\"success\":")
        && reply.append_bool(snapshot.success)
        && reply.append_raw(",\"overflow\":")
        && reply.append_bool(snapshot.overflowed)
        && reply.append_raw(",\"response\":")
        && reply.append_json_string(response)
        && reply.append_raw("}");

    if (!body_ok) {
        if (json_errors) {
            return GcodeReply(Status::InternalServerError, can_keep_alive);
        }
        return GcodeReply(Status::InternalServerError, can_keep_alive);
    }

    return reply;
}

void GcodeReply::step(std::string_view, bool, uint8_t *buffer, size_t buffer_size, Step &out) {
    const ConnectionHandling handling = can_keep_alive ? ConnectionHandling::ContentLengthKeep : ConnectionHandling::Close;
    size_t written = 0;

    if (!headers_sent) {
        written = write_headers(buffer, buffer_size, status, ContentType::ApplicationJson, handling, body_size);
        headers_sent = true;
    }

    const size_t remaining = body_size - body_offset;
    const size_t chunk = std::min(buffer_size - written, remaining);
    memcpy(buffer + written, body.data() + body_offset, chunk);
    body_offset += chunk;

    if (body_offset >= body_size) {
        out = Step { 0, written + chunk, Terminating::for_handling(handling) };
    } else {
        out = Step { 0, written + chunk, Continue() };
    }
}

} // namespace nhttp::printer
