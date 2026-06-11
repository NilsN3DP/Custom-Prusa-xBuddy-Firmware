#pragma once

#include "status_page.h"

#include <array>
#include <string_view>

namespace nhttp::printer {

class GcodeCommand {
private:
    static constexpr size_t BUFFER_LEN = 160;
    static constexpr size_t COMMAND_LEN = 96;

    std::array<uint8_t, BUFFER_LEN> buffer {};
    std::array<char, COMMAND_LEN> command {};
    size_t buffer_used = 0;
    size_t content_length;
    bool can_keep_alive;
    bool json_errors;

    handler::StatusPage process();
    bool submit_command(const char *gcode);

public:
    GcodeCommand(size_t content_length, bool can_keep_alive, bool json_errors);
    bool want_read() const { return true; }
    bool want_write() const { return false; }
    void step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size, handler::Step &out);
};

} // namespace nhttp::printer
