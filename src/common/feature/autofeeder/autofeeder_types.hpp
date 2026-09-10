/// @file
/// Shared value types for the automatic filament feeder (autofeeder).
///
/// The autofeeder is an external, motorised filament feeder that pushes filament
/// from the spool through the long PTFE tube up to the printer's side filament
/// sensor, and pulls it back out again on unload. It replaces the manual "push
/// the filament in until the side sensor sees it" step of the XL.
///
/// This header is deliberately free of any firmware dependencies so that it can
/// be shared with the feeder controller's own firmware and used in unit tests.

#pragma once

#include <cstdint>

namespace buddy::autofeeder {

/// Maximum number of feeder channels the protocol can address.
/// The number of channels actually present is reported by the controller
/// (see \p InfoResponse::channel_count) and is further limited by the number of
/// tools of the printer.
inline constexpr uint8_t max_channels = 5;

/// Direction of a feed movement, as seen from the printer.
enum class FeedDirection : uint8_t {
    /// Push filament towards the printer (spool -> extruder).
    forward = 0,

    /// Pull filament back towards the spool (extruder -> spool).
    backward = 1,
};

/// What a single feeder channel is currently doing.
///
/// Mirrors the phases of the Snapmaker U1 feeder (preload / load / unload) but
/// is reduced to what the printer actually has to distinguish: the arrival
/// detection lives in the printer (side filament sensor), not in the feeder.
enum class ChannelState : uint8_t {
    /// Controller has not reported anything about this channel yet.
    unknown = 0,

    /// No feeder module is plugged into this channel.
    not_present = 1,

    /// Module present, inlet port empty, motor stopped.
    idle = 2,

    /// Filament sits in the inlet port but has not been fed towards the printer.
    filament_at_inlet = 3,

    /// Motor is running forward.
    feeding = 4,

    /// Motor is running backward.
    retracting = 5,

    /// Last movement finished because the printer asked it to stop.
    stopped = 6,

    /// Last movement aborted, see \p ChannelStatus::error.
    failed = 7,
};

/// Why the last movement of a channel failed.
///
/// The error set is taken from the Snapmaker feeder implementation, which has
/// the benefit of being proven against the real mechanics.
enum class ChannelError : uint8_t {
    /// No error.
    ok = 0,

    /// Unspecified failure.
    general = 1,

    /// Inlet port reports no filament (it was pulled out mid-move).
    no_filament = 2,

    /// Motor tacho reports (almost) no rotation although the motor is driven.
    /// Usually a stalled motor or a broken tacho.
    motor_speed = 3,

    /// Motor turns but the pinch wheels do not follow, i.e. the filament slips.
    wheel_speed = 4,

    /// The movement did not finish within its timeout.
    timeout = 5,

    /// The requested length was fed but the printer never saw the filament
    /// arrive at the side filament sensor.
    distance = 6,

    /// The channel was asked to do something that does not match its state.
    state_mismatch = 7,
};

/// Whether \p error describes an actual failure.
constexpr bool is_error(ChannelError error) {
    return error != ChannelError::ok;
}

/// Whether the channel is currently moving filament.
constexpr bool is_moving(ChannelState state) {
    return state == ChannelState::feeding || state == ChannelState::retracting;
}

/// Whether the channel is in a state where a new movement may be started.
constexpr bool accepts_movement(ChannelState state) {
    switch (state) {
    case ChannelState::idle:
    case ChannelState::filament_at_inlet:
    case ChannelState::stopped:
    case ChannelState::failed:
        return true;

    case ChannelState::unknown:
    case ChannelState::not_present:
    case ChannelState::feeding:
    case ChannelState::retracting:
        return false;
    }
    return false;
}

/// Snapshot of one feeder channel, as reported by the controller.
struct ChannelStatus {
    ChannelState state = ChannelState::unknown;
    ChannelError error = ChannelError::ok;

    /// Filament is present in the inlet port of this channel.
    bool filament_at_inlet = false;

    /// Feeder motor of this channel is being driven.
    bool motor_running = false;

    /// Revolutions per minute of the drive motor (before the gearbox).
    uint16_t motor_rpm = 0;

    /// Revolutions per minute of the pinch wheels (after the gearbox).
    uint16_t wheel_rpm = 0;

    /// Filament length moved since the current/last movement was started, in
    /// hundredths of a millimetre. Positive means towards the printer.
    int32_t moved_mm_x100 = 0;

    /// Length moved since the current/last movement started, in millimetres.
    constexpr float moved_mm() const {
        return static_cast<float>(moved_mm_x100) / 100.0f;
    }

    bool operator==(const ChannelStatus &) const = default;
};

/// Identification of the attached feeder controller.
struct ControllerInfo {
    /// Version of the wire protocol the controller speaks.
    uint8_t protocol_version = 0;

    /// Number of channels the controller drives.
    uint8_t channel_count = 0;

    /// Controller firmware version.
    uint8_t fw_major = 0;
    uint8_t fw_minor = 0;

    bool operator==(const ControllerInfo &) const = default;
};

} // namespace buddy::autofeeder
