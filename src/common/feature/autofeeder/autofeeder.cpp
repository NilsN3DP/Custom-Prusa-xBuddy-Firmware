#include "autofeeder.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace buddy::autofeeder {

namespace {

    using Lock = std::unique_lock<freertos::Mutex>;

    /// Wrap-around safe "at least \p interval has passed since \p since".
    bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
        return static_cast<int32_t>(now - since) >= static_cast<int32_t>(interval);
    }

    template <typename T>
    std::span<const uint8_t> as_bytes(const T &value) {
        return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&value), sizeof(T));
    }

} // namespace

void AutoFeeder::set_transport(Transport *transport) {
    Lock lock(mutex_);

    transport_ = transport;
    rx_size_ = 0;
    in_flight_.reset();
    connected_ = false;
    info_ = {};
    channels_ = {};
}

bool AutoFeeder::is_connected() const {
    Lock lock(mutex_);
    return connected_;
}

ControllerInfo AutoFeeder::info() const {
    Lock lock(mutex_);
    return info_;
}

uint8_t AutoFeeder::channel_count() const {
    Lock lock(mutex_);
    return connected_ ? info_.channel_count : 0;
}

ChannelStatus AutoFeeder::status(uint8_t channel) const {
    Lock lock(mutex_);

    if (channel >= max_channels) {
        return {};
    }

    return channels_[channel].status;
}

bool AutoFeeder::has_pending_request(uint8_t channel) const {
    Lock lock(mutex_);

    if (channel >= max_channels) {
        return false;
    }

    return channels_[channel].queued.has_value()
        || (in_flight_.has_value() && in_flight_->channel == channel);
}

bool AutoFeeder::queue_command(uint8_t channel, Command command, std::span<const uint8_t> payload) {
    Lock lock(mutex_);

    if (!connected_ || channel >= info_.channel_count || channel >= max_channels) {
        return false;
    }

    QueuedCommand queued;
    queued.command = command;
    queued.payload_size = static_cast<uint8_t>(payload.size());
    std::copy(payload.begin(), payload.end(), queued.payload.begin());

    // A newer request for the same channel supersedes an older one that has not
    // been sent yet; this is what makes a stop override a not-yet-sent feed.
    channels_[channel].queued = queued;
    return true;
}

bool AutoFeeder::start_feed(uint8_t channel, FeedDirection direction, uint8_t duty, uint16_t max_length_mm, uint16_t timeout_ds) {
    const FeedRequest request {
        .channel = channel,
        .direction = static_cast<uint8_t>(direction),
        .duty = duty,
        .max_length_mm = max_length_mm,
        .timeout_ds = timeout_ds,
    };

    return queue_command(channel, Command::feed, as_bytes(request));
}

bool AutoFeeder::request_stop(uint8_t channel) {
    return queue_command(channel, Command::stop, std::span<const uint8_t>(&channel, 1));
}

bool AutoFeeder::set_led(uint8_t channel, uint8_t red, uint8_t white) {
    const SetLedRequest request {
        .channel = channel,
        .red = red,
        .white = white,
    };

    return queue_command(channel, Command::set_led, as_bytes(request));
}

void AutoFeeder::step(uint32_t now_ms) {
    Lock lock(mutex_);

    if (!transport_) {
        return;
    }

    pump_rx();

    if (in_flight_.has_value()) {
        if (!elapsed(now_ms, in_flight_->sent_at_ms, response_timeout_ms)) {
            return;
        }

        if (in_flight_->attempts < max_retries) {
            resend(now_ms);
        } else {
            in_flight_.reset();
            set_disconnected();
        }
        return;
    }

    send_next_request(now_ms);
}

void AutoFeeder::pump_rx() {
    // Make room by dropping the oldest bytes if the decoder could not keep up.
    // Losing a stale partial frame is harmless: the request will be retried.
    if (rx_size_ >= rx_buffer_.size()) {
        rx_size_ = 0;
    }

    const size_t read = transport_->read(std::span(rx_buffer_).subspan(rx_size_));
    rx_size_ += read;

    while (rx_size_ > 0) {
        DecodedFrame frame;
        size_t consumed = 0;
        const DecodeResult result = find_frame(std::span(rx_buffer_).first(rx_size_), frame, consumed);

        if (consumed > 0) {
            std::copy(rx_buffer_.begin() + consumed, rx_buffer_.begin() + rx_size_, rx_buffer_.begin());
            rx_size_ -= consumed;
        }

        if (result == DecodeResult::ok) {
            handle_response(frame);
            continue;
        }

        // no_sync / incomplete / bad_crc: nothing more to extract right now.
        // find_frame already told us what to discard.
        if (result == DecodeResult::bad_crc && consumed > 0) {
            continue;
        }
        break;
    }
}

void AutoFeeder::handle_response(const DecodedFrame &frame) {
    if (!frame.is_response || !in_flight_.has_value() || frame.seq != in_flight_->seq) {
        // Stale or unsolicited: the controller only ever speaks when spoken to.
        return;
    }

    const InFlight request = *in_flight_;
    in_flight_.reset();

    if (frame.command == Command::error) {
        // The controller understood us but refused. Nothing to retry.
        return;
    }

    if (frame.command != request.command) {
        return;
    }

    switch (frame.command) {

    case Command::info: {
        if (frame.payload.size() != sizeof(InfoResponse)) {
            return;
        }

        InfoResponse response;
        std::memcpy(&response, frame.payload.data(), sizeof(response));

        if (response.protocol_version != protocol_version || response.channel_count == 0) {
            // Incompatible controller; stay disconnected rather than talking nonsense to it.
            set_disconnected();
            return;
        }

        info_ = ControllerInfo {
            .protocol_version = response.protocol_version,
            .channel_count = std::min<uint8_t>(response.channel_count, max_channels),
            .fw_major = response.fw_major,
            .fw_minor = response.fw_minor,
        };
        connected_ = true;
        break;
    }

    case Command::get_status:
    case Command::feed:
    case Command::stop: {
        if (frame.payload.size() != sizeof(StatusResponse)) {
            return;
        }

        StatusResponse response;
        std::memcpy(&response, frame.payload.data(), sizeof(response));

        if (response.channel < max_channels) {
            channels_[response.channel].status = to_channel_status(response);
        }
        break;
    }

    case Command::set_led:
    case Command::error:
        break;
    }
}

bool AutoFeeder::send_next_request(uint32_t now_ms) {
    if (!connected_) {
        if (!elapsed(now_ms, last_connect_attempt_ms_, reconnect_interval_ms)) {
            return false;
        }

        last_connect_attempt_ms_ = now_ms;
        return send(Command::info, 0, {}, now_ms);
    }

    // Queued channel commands take priority over status polling.
    for (uint8_t offset = 0; offset < info_.channel_count; offset++) {
        const uint8_t channel = (poll_cursor_ + offset) % info_.channel_count;
        Channel &ch = channels_[channel];

        if (!ch.queued.has_value()) {
            continue;
        }

        const QueuedCommand queued = *ch.queued;
        ch.queued.reset();
        return send(queued.command, channel, std::span(queued.payload).first(queued.payload_size), now_ms);
    }

    const uint32_t interval = any_channel_moving() ? active_poll_interval_ms : idle_poll_interval_ms;
    if (!elapsed(now_ms, last_poll_ms_, interval)) {
        return false;
    }

    // While something is moving, keep polling that channel so that the printer
    // sees a stall or a runout within one poll interval.
    uint8_t channel = poll_cursor_;
    for (uint8_t offset = 0; offset < info_.channel_count; offset++) {
        const uint8_t candidate = (poll_cursor_ + offset) % info_.channel_count;
        if (is_moving(channels_[candidate].status.state)) {
            channel = candidate;
            break;
        }
    }

    if (!is_moving(channels_[channel].status.state)) {
        poll_cursor_ = static_cast<uint8_t>((poll_cursor_ + 1) % info_.channel_count);
    }

    last_poll_ms_ = now_ms;
    return send(Command::get_status, channel, std::span<const uint8_t>(&channel, 1), now_ms);
}

bool AutoFeeder::send(Command command, uint8_t channel, std::span<const uint8_t> payload, uint32_t now_ms) {
    const uint8_t seq = next_seq_++;

    tx_size_ = encode_frame(command, seq, payload, tx_buffer_);
    if (tx_size_ == 0) {
        return false;
    }

    if (transport_->write(std::span(tx_buffer_).first(tx_size_)) != tx_size_) {
        return false;
    }

    in_flight_ = InFlight {
        .command = command,
        .seq = seq,
        .channel = channel,
        .sent_at_ms = now_ms,
        .attempts = 1,
    };
    return true;
}

void AutoFeeder::resend(uint32_t now_ms) {
    transport_->write(std::span(tx_buffer_).first(tx_size_));
    in_flight_->sent_at_ms = now_ms;
    in_flight_->attempts++;
}

void AutoFeeder::set_disconnected() {
    connected_ = false;
    info_ = {};
    channels_ = {};
    rx_size_ = 0;
    poll_cursor_ = 0;
}

bool AutoFeeder::any_channel_moving() const {
    return std::any_of(channels_.begin(), channels_.end(), [](const Channel &channel) {
        return is_moving(channel.status.state);
    });
}

AutoFeeder &instance() {
    static AutoFeeder feeder;
    return feeder;
}

} // namespace buddy::autofeeder
