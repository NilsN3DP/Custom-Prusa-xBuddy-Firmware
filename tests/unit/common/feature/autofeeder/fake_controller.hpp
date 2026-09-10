/// @file
/// A fake autofeeder controller for the unit tests.
///
/// It speaks the real wire protocol so that the tests exercise framing, CRC and
/// sequence handling instead of a mocked-out shortcut.

#pragma once

#include <feature/autofeeder/autofeeder.hpp>
#include <feature/autofeeder/autofeeder_protocol.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <vector>

namespace test {

using namespace buddy::autofeeder;

class FakeController : public Transport {
public:
    struct ChannelSim {
        ChannelState state = ChannelState::idle;
        ChannelError error = ChannelError::ok;
        bool filament_at_inlet = false;
        uint16_t motor_rpm = 0;
        uint16_t wheel_rpm = 0;
        int32_t moved_mm_x100 = 0;

        /// Set by the fake when a feed is accepted.
        uint8_t last_duty = 0;
        uint16_t last_max_length_mm = 0;
        FeedDirection last_direction = FeedDirection::forward;
        int feed_count = 0;
        int stop_count = 0;
    };

    uint8_t channel_count = 2;
    uint8_t fw_major = 1;
    uint8_t fw_minor = 0;
    uint8_t protocol_version_to_report = buddy::autofeeder::protocol_version;

    /// While set, the controller stays silent (simulates a lost controller).
    bool mute = false;

    /// Corrupt the next response so that the driver sees a CRC error.
    bool corrupt_next_response = false;

    std::array<ChannelSim, max_channels> channels;

    /// Requests the fake has seen, in order.
    std::vector<Command> seen_commands;

    size_t write(std::span<const uint8_t> data) override {
        rx_.insert(rx_.end(), data.begin(), data.end());
        process();
        return data.size();
    }

    size_t read(std::span<uint8_t> data) override {
        const size_t n = std::min(data.size(), tx_.size());
        std::copy(tx_.begin(), tx_.begin() + n, data.begin());
        tx_.erase(tx_.begin(), tx_.begin() + n);
        return n;
    }

    /// Bytes still waiting to be picked up by the driver.
    size_t pending_bytes() const { return tx_.size(); }

private:
    void process() {
        while (!rx_.empty()) {
            const std::vector<uint8_t> buffer(rx_.begin(), rx_.end());
            DecodedFrame frame;
            size_t consumed = 0;
            const DecodeResult result = find_frame(buffer, frame, consumed);

            if (consumed > 0) {
                rx_.erase(rx_.begin(), rx_.begin() + consumed);
            }

            if (result != DecodeResult::ok) {
                break;
            }

            seen_commands.push_back(frame.command);
            if (!mute) {
                respond(frame);
            }
        }
    }

    void respond(const DecodedFrame &frame) {
        switch (frame.command) {

        case Command::info: {
            const InfoResponse response {
                .protocol_version = protocol_version_to_report,
                .channel_count = channel_count,
                .fw_major = fw_major,
                .fw_minor = fw_minor,
            };
            send(frame.command, frame.seq, { reinterpret_cast<const uint8_t *>(&response), sizeof(response) });
            break;
        }

        case Command::get_status: {
            if (frame.payload.size() != 1) {
                return;
            }
            send_status(frame.seq, frame.payload[0], frame.command);
            break;
        }

        case Command::feed: {
            if (frame.payload.size() != sizeof(FeedRequest)) {
                return;
            }
            FeedRequest request;
            std::memcpy(&request, frame.payload.data(), sizeof(request));

            if (request.channel < max_channels) {
                ChannelSim &sim = channels[request.channel];
                sim.feed_count++;
                sim.last_duty = request.duty;
                sim.last_max_length_mm = request.max_length_mm;
                sim.last_direction = static_cast<FeedDirection>(request.direction);
                sim.state = (sim.last_direction == FeedDirection::forward) ? ChannelState::feeding : ChannelState::retracting;
                sim.error = ChannelError::ok;
                sim.motor_rpm = 3000;
                sim.wheel_rpm = 90;
            }
            send_status(frame.seq, request.channel, frame.command);
            break;
        }

        case Command::stop: {
            if (frame.payload.size() != 1) {
                return;
            }
            const uint8_t channel = frame.payload[0];
            if (channel < max_channels) {
                ChannelSim &sim = channels[channel];
                sim.stop_count++;
                sim.state = ChannelState::stopped;
                sim.motor_rpm = 0;
                sim.wheel_rpm = 0;
            }
            send_status(frame.seq, channel, frame.command);
            break;
        }

        case Command::set_led:
            send(frame.command, frame.seq, {});
            break;

        case Command::error:
            break;
        }
    }

    void send_status(uint8_t seq, uint8_t channel, Command command) {
        if (channel >= max_channels) {
            const uint8_t error = static_cast<uint8_t>(ProtocolError::bad_channel);
            send(Command::error, seq, { &error, 1 });
            return;
        }

        const ChannelSim &sim = channels[channel];
        StatusResponse response {
            .channel = channel,
            .state = static_cast<uint8_t>(sim.state),
            .error = static_cast<uint8_t>(sim.error),
            .flags = static_cast<uint8_t>((sim.filament_at_inlet ? status_flag_filament_at_inlet : 0)
                | (is_moving(sim.state) ? status_flag_motor_running : 0)),
            .motor_rpm = sim.motor_rpm,
            .wheel_rpm = sim.wheel_rpm,
            .moved_mm_x100 = sim.moved_mm_x100,
        };
        send(command, seq, { reinterpret_cast<const uint8_t *>(&response), sizeof(response) });
    }

    void send(Command command, uint8_t seq, std::span<const uint8_t> payload) {
        std::array<uint8_t, max_frame_size> buffer;
        const size_t size = encode_frame(static_cast<Command>(static_cast<uint8_t>(command) | response_flag), seq, payload, buffer);

        if (corrupt_next_response && size > 0) {
            corrupt_next_response = false;
            buffer[size - 1] ^= 0xFF;
        }

        tx_.insert(tx_.end(), buffer.begin(), buffer.begin() + size);
    }

    std::vector<uint8_t> rx_;
    std::vector<uint8_t> tx_;
};

/// Runs the driver until \p predicate holds or \p max_steps have elapsed.
/// \returns the simulated time in milliseconds when the loop ended.
template <typename Predicate>
uint32_t run_until(AutoFeeder &feeder, uint32_t &now_ms, Predicate predicate, uint32_t step_ms = 5, int max_steps = 4000) {
    for (int i = 0; i < max_steps; i++) {
        if (predicate()) {
            return now_ms;
        }
        feeder.step(now_ms);
        now_ms += step_ms;
    }
    return now_ms;
}

} // namespace test
