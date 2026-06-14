#pragma once

#include "step.h"
#include <http/types.h>
#include <marlin_server.hpp>

#include <array>
#include <string_view>

namespace nhttp::printer {

class GcodeReply {
    static constexpr size_t BODY_BUFFER_SIZE = 2400;

    std::array<char, BODY_BUFFER_SIZE> body {};
    size_t body_size = 0;
    size_t body_offset = 0;
    http::Status status = http::Status::Ok;
    bool can_keep_alive = false;
    bool headers_sent = false;

    GcodeReply(http::Status status, bool can_keep_alive);
    bool append_raw(std::string_view text);
    bool append_json_string(std::string_view text);
    bool append_bool(bool value);
    bool append_uint(uint32_t value);

public:
    static GcodeReply submitted(uint32_t id, bool can_keep_alive);
    static GcodeReply from_capture(uint32_t id, bool can_keep_alive, bool json_errors);

    bool want_read() const { return false; }
    bool want_write() const { return headers_sent == false || body_offset < body_size; }
    void step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size, handler::Step &out);
};

} // namespace nhttp::printer
