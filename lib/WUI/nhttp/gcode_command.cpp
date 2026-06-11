#include "gcode_command.h"
#include "handler.h"
#include "json_parser.h"

#include <marlin_client.hpp>

#include <algorithm>
#include <cstring>

namespace nhttp::printer {

using namespace handler;
using http::Status;
using json::Event;
using json::Type;
using std::nullopt;

namespace {

    bool is_space(char c) {
        return c == ' ' || c == '\t';
    }

    const char *trim_left(const char *str) {
        while (is_space(*str)) {
            ++str;
        }
        return str;
    }

    bool is_allowed_command(const char *gcode) {
        gcode = trim_left(gcode);
        return strncmp(gcode, "M150", 4) == 0 || strncmp(gcode, "M151", 4) == 0 || strncmp(gcode, "M152", 4) == 0 || strncmp(gcode, "M153", 4) == 0;
    }

    bool has_line_break(const char *gcode) {
        return strchr(gcode, '\n') != nullptr || strchr(gcode, '\r') != nullptr;
    }

} // namespace

GcodeCommand::GcodeCommand(size_t content_length, bool can_keep_alive, bool json_errors)
    : content_length(content_length)
    , can_keep_alive(can_keep_alive)
    , json_errors(json_errors) {
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

    out = Step { to_read, 0, process() };
}

StatusPage GcodeCommand::process() {
    bool command_seen = false;
    bool command_too_long = false;

    const auto parse_result = parse_command(reinterpret_cast<char *>(buffer.data()), buffer_used, [&](const Event &event) {
        if (event.depth != 1 || event.type != Type::String || event.key != "command") {
            return;
        }

        const auto &value = event.value.value();
        if (value.size() >= command.size()) {
            command_too_long = true;
            return;
        }

        memcpy(command.data(), value.data(), value.size());
        command[value.size()] = '\0';
        command_seen = true;
    });

    switch (parse_result) {
    case JsonParseResult::ErrMem:
        return StatusPage(Status::PayloadTooLarge, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Too many JSON tokens");
    case JsonParseResult::ErrReq:
        return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Couldn't parse JSON");
    case JsonParseResult::Ok:
        break;
    }

    if (command_too_long) {
        return StatusPage(Status::PayloadTooLarge, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "GCode too long");
    }

    if (!command_seen) {
        return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Missing command");
    }

    if (!is_allowed_command(command.data()) || has_line_break(command.data())) {
        return StatusPage(Status::BadRequest, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "Only M150/M151/M152/M153 light and fan commands are allowed");
    }

    if (submit_command(command.data())) {
        return StatusPage(Status::NoContent, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors);
    }
    return StatusPage(Status::Conflict, can_keep_alive ? StatusPage::CloseHandling::KeepAlive : StatusPage::CloseHandling::Close, json_errors, nullopt, "GCode queue is busy");
}

bool GcodeCommand::submit_command(const char *gcode) {
    return marlin_client::gcode_try(gcode) == marlin_client::GcodeTryResult::Submitted;
}

} // namespace nhttp::printer
