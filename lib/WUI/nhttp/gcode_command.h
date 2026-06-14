#pragma once

#include "handler.h"
#include <array>

namespace nhttp::printer {

class GcodeCommand final : public handler::Handler {
    std::array<uint8_t, 256> buffer;
    size_t buffer_used = 0;
    const size_t content_length;
    const bool can_keep_alive;
    const bool json_errors;

public:
    GcodeCommand(size_t content_length, bool can_keep_alive, bool json_errors);
    virtual void step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size, handler::Step &out) override;
};

} // namespace nhttp::printer
