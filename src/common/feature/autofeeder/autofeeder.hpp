/// @file
/// Driver for the external automatic filament feeder controller.
///
/// The driver is a plain request/response state machine on top of \p Transport.
/// It never blocks: \p AutoFeeder::step() is called periodically from the Marlin
/// server task and does at most one transport read and one write per call.
///
/// Deciding *when* to feed is not this class's job. On the XL the "filament has
/// arrived" signal is the side filament sensor, which is read by the mainboard,
/// so the load/unload orchestration lives in the pause state machine and merely
/// drives this class (see \p autofeeder_load.hpp).

#pragma once

#include "autofeeder_protocol.hpp"
#include "autofeeder_types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include <freertos/mutex.hpp>

namespace buddy::autofeeder {

/// Byte pipe to the feeder controller.
class Transport {
public:
    virtual ~Transport() = default;

    /// Sends up to \p data.size() bytes. \returns how many were accepted.
    virtual size_t write(std::span<const uint8_t> data) = 0;

    /// Reads up to \p data.size() bytes without blocking. \returns how many were read.
    virtual size_t read(std::span<uint8_t> data) = 0;
};

class AutoFeeder {
public:
    /// How long to wait for a response before retrying.
    static constexpr uint32_t response_timeout_ms = 100;

    /// Retries of a single request before the controller counts as lost.
    static constexpr uint8_t max_retries = 3;

    /// Interval between reconnection attempts while disconnected.
    static constexpr uint32_t reconnect_interval_ms = 500;

    /// Status poll interval while at least one channel is moving.
    static constexpr uint32_t active_poll_interval_ms = 20;

    /// Status poll interval while nothing moves.
    static constexpr uint32_t idle_poll_interval_ms = 250;

    /// Binds the driver to \p transport. Passing nullptr keeps the driver inert,
    /// which is what a printer without a feeder attached uses.
    void set_transport(Transport *transport);

    /// Advances the state machine. \p now_ms is a free-running millisecond clock.
    void step(uint32_t now_ms);

    /// Whether the controller answered recently.
    bool is_connected() const;

    /// Identification of the controller. Only meaningful while connected.
    ControllerInfo info() const;

    /// Number of channels the attached controller drives, 0 while disconnected.
    uint8_t channel_count() const;

    /// Last known status of \p channel. Returns a \p ChannelState::unknown status
    /// for out-of-range channels.
    ChannelStatus status(uint8_t channel) const;

    /// Requests a movement on \p channel.
    ///
    /// The controller stops on its own after \p max_length_mm or \p timeout_ds,
    /// whichever comes first, so a printer that stops talking cannot leave the
    /// motor running. Call \p request_stop() to end the movement earlier.
    ///
    /// \returns false if the channel does not exist or is not connected.
    bool start_feed(uint8_t channel, FeedDirection direction, uint8_t duty, uint16_t max_length_mm, uint16_t timeout_ds);

    /// Asks \p channel to stop moving.
    bool request_stop(uint8_t channel);

    /// Sets the status LEDs of \p channel.
    bool set_led(uint8_t channel, uint8_t red, uint8_t white);

    /// Whether a request for \p channel is queued or in flight.
    bool has_pending_request(uint8_t channel) const;

private:
    struct QueuedCommand {
        Command command;

        /// Payload of the command, sized for the largest one.
        std::array<uint8_t, sizeof(FeedRequest)> payload;
        uint8_t payload_size;
    };

    struct Channel {
        ChannelStatus status;
        std::optional<QueuedCommand> queued;
    };

    struct InFlight {
        Command command;
        uint8_t seq;
        uint8_t channel;
        uint32_t sent_at_ms;
        uint8_t attempts;
    };

    /// Reads whatever the transport has and processes complete frames.
    void pump_rx();

    /// Handles a decoded response frame.
    void handle_response(const DecodedFrame &frame);

    /// Picks and sends the next request, if any. \returns whether one was sent.
    bool send_next_request(uint32_t now_ms);

    /// Sends \p command with \p payload and records it as in flight.
    bool send(Command command, uint8_t channel, std::span<const uint8_t> payload, uint32_t now_ms);

    /// Resends the in-flight request.
    void resend(uint32_t now_ms);

    /// Drops the connection and forgets all channel state.
    void set_disconnected();

    /// Whether any channel is currently moving filament.
    bool any_channel_moving() const;

    /// Queues \p command for \p channel, replacing anything queued before.
    bool queue_command(uint8_t channel, Command command, std::span<const uint8_t> payload);

    mutable freertos::Mutex mutex_;

    Transport *transport_ = nullptr;

    bool connected_ = false;
    ControllerInfo info_;

    std::array<Channel, max_channels> channels_;

    std::optional<InFlight> in_flight_;
    uint8_t next_seq_ = 0;

    /// Channel whose status is polled next.
    uint8_t poll_cursor_ = 0;

    uint32_t last_poll_ms_ = 0;
    uint32_t last_connect_attempt_ms_ = 0;

    /// Bytes received but not yet consumed by the frame decoder.
    std::array<uint8_t, 2 * max_frame_size> rx_buffer_;
    size_t rx_size_ = 0;

    /// Buffer for the frame currently being sent. Kept around for retries.
    std::array<uint8_t, max_frame_size> tx_buffer_;
    size_t tx_size_ = 0;
};

/// The printer's feeder instance.
///
/// Named \p instance rather than \p autofeeder because the namespace already
/// carries that name.
AutoFeeder &instance();

} // namespace buddy::autofeeder
