/// @file
/// Load/unload orchestration on top of the \p AutoFeeder device driver.
///
/// The feeder itself only knows "run the motor forward/backward for at most N
/// millimetres or N tenths of a second". Deciding that the filament has arrived
/// is the printer's job, because the sensor that sees it - the XL's side
/// filament sensor - is read by the mainboard, not by the feeder.
///
/// \p FeedOperation implements that outer loop. It is deliberately free of any
/// Marlin/sensor includes: the state of the target sensor is passed into
/// \p step(), which makes the whole sequence unit-testable.

#pragma once

#include "autofeeder.hpp"
#include "autofeeder_types.hpp"

#include <cstdint>

namespace buddy::autofeeder {

/// Motor drive levels, ported from the Snapmaker U1 feeder where they are
/// expressed as PWM duty cycles in the range 0..1.
///
/// Snapmaker runs the filament into the drive gears slowly, then feeds at 70 %
/// and ramps up if the filament does not arrive, which is what \p FeedOperation
/// reproduces.
inline constexpr uint8_t duty_engage = 115; ///< 0.45 - pull filament into the drive wheels
inline constexpr uint8_t duty_feed = 179; ///< 0.70 - normal feeding speed
inline constexpr uint8_t duty_max = 255; ///< 1.00 - last resort before giving up

/// Length of the path from the feeder inlet to the printer's side sensor, plus
/// reserve. Snapmaker's own value for the U1 is 950 mm of preload with a 1100 mm
/// hard limit; the XL's tubes are of a comparable length.
inline constexpr uint16_t default_load_length_mm = 1100;

/// Extra length pulled back after the side sensor has gone clear, to get the
/// filament tip all the way back to the feeder inlet.
inline constexpr uint16_t default_retract_clear_length_mm = 1200;

/// Overall timeout of one load, in tenths of a second (Snapmaker uses 60 s).
inline constexpr uint16_t load_timeout_ds = 600;

/// Overall timeout of one retract, in tenths of a second.
inline constexpr uint16_t retract_timeout_ds = 600;

/// How long a single duty step of the initial ramp runs before the next one is
/// tried. Snapmaker uses 350 ms per step.
inline constexpr uint32_t duty_ramp_step_ms = 350;

/// Outcome of \p FeedOperation::step().
enum class FeedResult : uint8_t {
    /// Nothing is running.
    idle,

    /// The operation is still under way.
    busy,

    /// Load: the filament reached the target sensor.
    /// Retract: the filament is back at the feeder inlet.
    finished,

    /// No feeder is attached, or the requested channel does not exist.
    unavailable,

    /// The inlet port went empty - the user pulled the filament out, or the
    /// spool ran out.
    no_filament,

    /// The feeder reported a mechanical failure (stalled motor or slipping
    /// filament); see \p FeedOperation::error().
    feeder_error,

    /// The full length was moved but the target sensor never changed state.
    /// Typically a blocked tube or a mis-calibrated sensor.
    not_arrived,

    /// The operation did not finish within its timeout.
    timeout,
};

constexpr bool is_finished(FeedResult result) {
    return result != FeedResult::busy;
}

constexpr bool is_failure(FeedResult result) {
    switch (result) {
    case FeedResult::unavailable:
    case FeedResult::no_filament:
    case FeedResult::feeder_error:
    case FeedResult::not_arrived:
    case FeedResult::timeout:
        return true;

    case FeedResult::idle:
    case FeedResult::busy:
    case FeedResult::finished:
        return false;
    }
    return false;
}

/// Drives one load or retract on one feeder channel.
///
/// Only one operation may run at a time; starting a new one replaces the old.
class FeedOperation {
public:
    explicit FeedOperation(AutoFeeder &feeder)
        : feeder_(feeder) {}

    /// Pushes filament from the feeder inlet until the target sensor sees it.
    ///
    /// \returns false if the channel is unavailable, in which case the operation
    ///          stays idle.
    bool start_load(uint8_t channel, uint32_t now_ms, uint16_t max_length_mm = default_load_length_mm);

    /// Pulls filament back until the target sensor goes clear, then a further
    /// \p clear_length_mm to get the tip back to the inlet.
    bool start_retract(uint8_t channel, uint32_t now_ms, uint16_t clear_length_mm = default_retract_clear_length_mm);

    /// Advances the operation.
    ///
    /// \param now_ms                  free-running millisecond clock
    /// \param target_sensor_has_filament  current reading of the sensor the
    ///                                filament is being fed to (the side
    ///                                filament sensor of the tool)
    FeedResult step(uint32_t now_ms, bool target_sensor_has_filament);

    /// Stops the feeder and returns to idle.
    void abort();

    /// Result of the last \p step().
    FeedResult result() const { return result_; }

    /// Feeder-reported error of the last failure.
    ChannelError error() const { return error_; }

    /// Channel the current/last operation runs on.
    uint8_t channel() const { return channel_; }

    bool is_busy() const { return result_ == FeedResult::busy; }

private:
    enum class Phase : uint8_t {
        idle,

        /// Waiting for the feeder to confirm that the motor is running.
        starting,

        /// Feeding towards the target sensor, ramping the duty up if needed.
        loading,

        /// Retracting until the target sensor goes clear.
        retracting_to_sensor,

        /// Retracting the remaining length back to the inlet.
        retracting_to_inlet,

        /// Waiting for the motor to come to a stop before reporting the result.
        stopping,
    };

    FeedResult finish(FeedResult result);
    FeedResult begin_stop(FeedResult pending_result, uint32_t now_ms);

    /// Translates a feeder-reported channel error into a \p FeedResult.
    static FeedResult classify(ChannelError error);

    AutoFeeder &feeder_;

    Phase phase_ = Phase::idle;
    FeedResult result_ = FeedResult::idle;
    FeedResult pending_result_ = FeedResult::idle;
    ChannelError error_ = ChannelError::ok;

    uint8_t channel_ = 0;
    uint8_t duty_ = duty_feed;

    /// Whether the running operation pulls filament back rather than pushing it.
    bool retracting_ = false;

    uint16_t length_mm_ = 0;

    uint32_t phase_started_ms_ = 0;
    uint32_t last_ramp_ms_ = 0;
};

} // namespace buddy::autofeeder
