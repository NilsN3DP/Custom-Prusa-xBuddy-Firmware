/// @file
/// M1720 - report and manually drive the automatic filament feeder.

#include "../../PrusaGcodeSuite.hpp"

#include <feature/autofeeder/autofeeder.hpp>
#include <feature/autofeeder/autofeeder_load.hpp>

#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/gcode/parser.h>
#include <Marlin/src/core/serial.h>

using namespace buddy::autofeeder;

namespace {

const char *to_string(ChannelState state) {
    switch (state) {
    case ChannelState::unknown:
        return "unknown";
    case ChannelState::not_present:
        return "not_present";
    case ChannelState::idle:
        return "idle";
    case ChannelState::filament_at_inlet:
        return "at_inlet";
    case ChannelState::feeding:
        return "feeding";
    case ChannelState::retracting:
        return "retracting";
    case ChannelState::stopped:
        return "stopped";
    case ChannelState::failed:
        return "failed";
    }
    return "?";
}

const char *to_string(ChannelError error) {
    switch (error) {
    case ChannelError::ok:
        return "ok";
    case ChannelError::general:
        return "general";
    case ChannelError::no_filament:
        return "no_filament";
    case ChannelError::motor_speed:
        return "motor_speed";
    case ChannelError::wheel_speed:
        return "wheel_speed";
    case ChannelError::timeout:
        return "timeout";
    case ChannelError::distance:
        return "distance";
    case ChannelError::state_mismatch:
        return "state_mismatch";
    }
    return "?";
}

void report() {
    if (!instance().is_connected()) {
        SERIAL_ECHOLNPGM("autofeeder: not connected");
        return;
    }

    const ControllerInfo info = instance().info();
    SERIAL_ECHOPGM("autofeeder: connected fw ");
    SERIAL_ECHO(info.fw_major);
    SERIAL_CHAR('.');
    SERIAL_ECHO(info.fw_minor);
    SERIAL_ECHOPGM(" channels ");
    SERIAL_ECHOLN(info.channel_count);

    for (uint8_t channel = 0; channel < info.channel_count; channel++) {
        const ChannelStatus status = instance().status(channel);
        SERIAL_ECHOPGM("  ch");
        SERIAL_ECHO(channel);
        SERIAL_ECHOPGM(": ");
        SERIAL_ECHO(to_string(status.state));
        SERIAL_ECHOPGM(" err=");
        SERIAL_ECHO(to_string(status.error));
        SERIAL_ECHOPGM(" inlet=");
        SERIAL_ECHO(status.filament_at_inlet ? 1 : 0);
        SERIAL_ECHOPGM(" motor_rpm=");
        SERIAL_ECHO(status.motor_rpm);
        SERIAL_ECHOPGM(" wheel_rpm=");
        SERIAL_ECHO(status.wheel_rpm);
        SERIAL_ECHOPGM(" moved_mm=");
        SERIAL_ECHOLN(status.moved_mm());
    }
}

} // namespace

/** \addtogroup G-Codes
 * @{
 */

/**
 * @brief Report the state of the automatic filament feeder, or drive it by hand.
 *
 * Without parameters the state of the controller and all its channels is
 * reported. This is the way to check the wiring of a freshly connected feeder.
 *
 * The movement parameters are meant for bring-up and diagnostics; normal loading
 * and unloading drives the feeder on its own.
 *
 * ## Parameters
 *
 * - `P` - channel to act on, defaults to the active tool
 * - `L` - feed this many millimetres towards the printer (negative pulls back)
 * - `S` - drive level 0..255, defaults to the normal feeding speed
 * - `X` - stop the channel
 *
 * Movement always stops on its own after the requested length or after the
 * feeder's own timeout, whichever comes first.
 */
void PrusaGcodeSuite::M1720() {
    if (!parser.seen("PLSX")) {
        report();
        return;
    }

    const uint8_t channel = parser.byteval('P', active_extruder);

    if (parser.seen('X')) {
        if (!instance().request_stop(channel)) {
            SERIAL_ECHOLNPGM("autofeeder: channel unavailable");
        }
        return;
    }

    if (!parser.seen('L')) {
        report();
        return;
    }

    const float length = parser.value_float();
    const uint8_t duty = parser.byteval('S', duty_feed);
    const auto direction = (length < 0) ? FeedDirection::backward : FeedDirection::forward;
    const auto length_mm = static_cast<uint16_t>(std::min(std::abs(length), 65535.0f));

    if (!instance().start_feed(channel, direction, duty, length_mm, load_timeout_ds)) {
        SERIAL_ECHOLNPGM("autofeeder: channel unavailable");
    }
}

/** @}*/
