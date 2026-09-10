#include "autofeeder_load.hpp"

#include <algorithm>
#include <cmath>

namespace buddy::autofeeder {

namespace {

    bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
        return static_cast<int32_t>(now - since) >= static_cast<int32_t>(interval);
    }

    /// How much of \p total is left after \p status reported its progress.
    ///
    /// Re-issuing a feed gives the channel a fresh length budget, so the length
    /// already moved has to be subtracted to keep the overall limit intact.
    uint16_t remaining_length_mm(uint16_t total, const ChannelStatus &status) {
        const float moved = std::abs(status.moved_mm());
        if (moved >= static_cast<float>(total)) {
            return 1;
        }
        return static_cast<uint16_t>(static_cast<float>(total) - moved);
    }

    /// Timeout of a phase in milliseconds, derived from the protocol's tenths of
    /// a second. A little longer than the feeder's own watchdog so that the
    /// feeder gets the chance to report the failure itself.
    constexpr uint32_t phase_timeout_ms(uint16_t timeout_ds) {
        return static_cast<uint32_t>(timeout_ds) * 100 + 2000;
    }

} // namespace

bool FeedOperation::start_load(uint8_t channel, uint32_t now_ms, uint16_t max_length_mm) {
    if (!feeder_.start_feed(channel, FeedDirection::forward, duty_engage, max_length_mm, load_timeout_ds)) {
        phase_ = Phase::idle;
        result_ = FeedResult::unavailable;
        return false;
    }

    channel_ = channel;
    length_mm_ = max_length_mm;
    duty_ = duty_engage;
    retracting_ = false;
    error_ = ChannelError::ok;
    phase_ = Phase::starting;
    result_ = FeedResult::busy;
    pending_result_ = FeedResult::idle;
    phase_started_ms_ = now_ms;
    last_ramp_ms_ = now_ms;
    return true;
}

bool FeedOperation::start_retract(uint8_t channel, uint32_t now_ms, uint16_t clear_length_mm) {
    if (!feeder_.start_feed(channel, FeedDirection::backward, duty_feed, clear_length_mm, retract_timeout_ds)) {
        phase_ = Phase::idle;
        result_ = FeedResult::unavailable;
        return false;
    }

    channel_ = channel;
    length_mm_ = clear_length_mm;
    duty_ = duty_feed;
    retracting_ = true;
    error_ = ChannelError::ok;
    phase_ = Phase::starting;
    result_ = FeedResult::busy;
    pending_result_ = FeedResult::idle;
    phase_started_ms_ = now_ms;
    last_ramp_ms_ = now_ms;
    return true;
}

void FeedOperation::abort() {
    if (phase_ != Phase::idle) {
        feeder_.request_stop(channel_);
    }

    phase_ = Phase::idle;
    result_ = FeedResult::idle;
    pending_result_ = FeedResult::idle;
}

FeedResult FeedOperation::finish(FeedResult result) {
    phase_ = Phase::idle;
    result_ = result;
    return result_;
}

FeedResult FeedOperation::begin_stop(FeedResult pending_result, uint32_t now_ms) {
    feeder_.request_stop(channel_);
    pending_result_ = pending_result;
    phase_ = Phase::stopping;
    phase_started_ms_ = now_ms;
    return result_;
}

FeedResult FeedOperation::classify(ChannelError error) {
    switch (error) {
    case ChannelError::ok:
        return FeedResult::finished;

    case ChannelError::no_filament:
        return FeedResult::no_filament;

    case ChannelError::timeout:
        return FeedResult::timeout;

    case ChannelError::distance:
        return FeedResult::not_arrived;

    case ChannelError::general:
    case ChannelError::motor_speed:
    case ChannelError::wheel_speed:
    case ChannelError::state_mismatch:
        return FeedResult::feeder_error;
    }

    return FeedResult::feeder_error;
}

FeedResult FeedOperation::step(uint32_t now_ms, bool target_sensor_has_filament) {
    if (phase_ == Phase::idle) {
        return result_;
    }

    const ChannelStatus status = feeder_.status(channel_);

    // Losing the controller mid-move is a failure of the operation; the feeder's
    // own watchdog stops the motor.
    if (!feeder_.is_connected()) {
        return finish(FeedResult::unavailable);
    }

    switch (phase_) {

    case Phase::idle:
        break;

    case Phase::starting:
        if (is_moving(status.state)) {
            phase_ = retracting_ ? Phase::retracting_to_sensor : Phase::loading;
            phase_started_ms_ = now_ms;
            last_ramp_ms_ = now_ms;
            break;
        }

        if (status.state == ChannelState::failed) {
            error_ = status.error;
            return finish(classify(status.error));
        }

        // The request may still be queued; give it a moment.
        if (elapsed(now_ms, phase_started_ms_, 1000) && !feeder_.has_pending_request(channel_)) {
            return finish(FeedResult::feeder_error);
        }
        break;

    case Phase::loading:
        if (target_sensor_has_filament) {
            return begin_stop(FeedResult::finished, now_ms);
        }

        if (!status.filament_at_inlet) {
            return begin_stop(FeedResult::no_filament, now_ms);
        }

        if (status.state == ChannelState::failed) {
            error_ = status.error;
            return finish(classify(status.error));
        }

        // The feeder stopped on its own: it moved the whole length without the
        // filament ever showing up at the sensor.
        if (!is_moving(status.state)) {
            return finish(FeedResult::not_arrived);
        }

        if (elapsed(now_ms, phase_started_ms_, phase_timeout_ms(load_timeout_ds))) {
            return begin_stop(FeedResult::timeout, now_ms);
        }

        // Ramp the drive level up while the filament has not arrived yet, the
        // same way the Snapmaker feeder does when the filament is stiff or the
        // tube has a lot of friction.
        if (duty_ < duty_max && elapsed(now_ms, last_ramp_ms_, duty_ramp_step_ms)) {
            duty_ = (duty_ < duty_feed) ? duty_feed : static_cast<uint8_t>(std::min<int>(duty_ + 26, duty_max));
            last_ramp_ms_ = now_ms;
            feeder_.start_feed(channel_, FeedDirection::forward, duty_, remaining_length_mm(length_mm_, status), load_timeout_ds);
        }
        break;

    case Phase::retracting_to_sensor:
        if (status.state == ChannelState::failed) {
            error_ = status.error;
            return finish(classify(status.error));
        }

        if (!target_sensor_has_filament) {
            // Sensor is clear; keep pulling to get the tip back to the inlet.
            phase_ = Phase::retracting_to_inlet;
            phase_started_ms_ = now_ms;
            feeder_.start_feed(channel_, FeedDirection::backward, duty_feed, length_mm_, retract_timeout_ds);
            break;
        }

        if (!is_moving(status.state)) {
            // Moved the whole length and the sensor still sees filament.
            return finish(FeedResult::not_arrived);
        }

        if (elapsed(now_ms, phase_started_ms_, phase_timeout_ms(retract_timeout_ds))) {
            return begin_stop(FeedResult::timeout, now_ms);
        }
        break;

    case Phase::retracting_to_inlet:
        if (status.state == ChannelState::failed) {
            error_ = status.error;
            return finish(classify(status.error));
        }

        // The feeder stops itself once the requested length is done. Ignore a
        // stop that is only the previous movement ending while the follow-up
        // feed is still on its way to the controller.
        if (!is_moving(status.state) && !feeder_.has_pending_request(channel_)) {
            return finish(FeedResult::finished);
        }

        if (elapsed(now_ms, phase_started_ms_, phase_timeout_ms(retract_timeout_ds))) {
            return begin_stop(FeedResult::timeout, now_ms);
        }
        break;

    case Phase::stopping:
        if (!is_moving(status.state)) {
            return finish(pending_result_);
        }

        // Do not wait forever for a motor that refuses to stop.
        if (elapsed(now_ms, phase_started_ms_, phase_timeout_ms(load_timeout_ds))) {
            return finish(pending_result_);
        }
        break;
    }

    return result_;
}

} // namespace buddy::autofeeder
