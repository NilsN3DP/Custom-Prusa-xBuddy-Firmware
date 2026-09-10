#include "autofeeder_protocol.hpp"

#include <algorithm>
#include <cstring>

namespace buddy::autofeeder {

namespace {

    /// Offset of the first CRC-covered byte within a frame.
    constexpr size_t crc_start_offset = 2;

    /// Offset of the payload within a frame.
    constexpr size_t payload_offset = 5;

    uint16_t read_u16(const uint8_t *data) {
        return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1]) << 8;
    }

    void write_u16(uint8_t *data, uint16_t value) {
        data[0] = static_cast<uint8_t>(value & 0xFF);
        data[1] = static_cast<uint8_t>(value >> 8);
    }

} // namespace

uint16_t crc16(std::span<const uint8_t> data) {
    uint16_t crc = 0xFFFF;

    for (const uint8_t byte : data) {
        crc ^= static_cast<uint16_t>(byte) << 8;

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>(crc << 1) ^ 0x1021;
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }

    return crc;
}

size_t encode_frame(Command command, uint8_t seq, std::span<const uint8_t> payload, std::span<uint8_t> buffer) {
    if (payload.size() > max_payload_size) {
        return 0;
    }

    const size_t frame_size = frame_overhead + payload.size();
    if (buffer.size() < frame_size) {
        return 0;
    }

    buffer[0] = sync_byte_0;
    buffer[1] = sync_byte_1;
    buffer[2] = static_cast<uint8_t>(command);
    buffer[3] = seq;
    buffer[4] = static_cast<uint8_t>(payload.size());

    std::copy(payload.begin(), payload.end(), buffer.begin() + payload_offset);

    const size_t crc_len = payload_offset + payload.size() - crc_start_offset;
    write_u16(buffer.data() + payload_offset + payload.size(), crc16(buffer.subspan(crc_start_offset, crc_len)));

    return frame_size;
}

DecodeResult find_frame(std::span<const uint8_t> buffer, DecodedFrame &frame, size_t &consumed) {
    consumed = 0;

    for (size_t start = 0; start + 1 < buffer.size(); start++) {
        if (buffer[start] != sync_byte_0 || buffer[start + 1] != sync_byte_1) {
            continue;
        }

        consumed = start;

        // Not enough bytes to even read the length field yet.
        if (buffer.size() - start < payload_offset) {
            return DecodeResult::incomplete;
        }

        const uint8_t raw_command = buffer[start + 2];
        const uint8_t seq = buffer[start + 3];
        const size_t payload_size = buffer[start + 4];

        // An overlong length field cannot come from a valid frame. Skip the sync
        // bytes so that a sync sequence contained in the garbage can still be found.
        if (payload_size > max_payload_size) {
            consumed = start + 2;
            return DecodeResult::bad_crc;
        }

        const size_t frame_size = frame_overhead + payload_size;
        if (buffer.size() - start < frame_size) {
            return DecodeResult::incomplete;
        }

        const size_t crc_len = payload_offset + payload_size - crc_start_offset;
        const uint16_t expected = crc16(buffer.subspan(start + crc_start_offset, crc_len));
        const uint16_t actual = read_u16(buffer.data() + start + payload_offset + payload_size);

        if (expected != actual) {
            consumed = start + 2;
            return DecodeResult::bad_crc;
        }

        frame.command = static_cast<Command>(raw_command & ~response_flag);
        frame.seq = seq;
        frame.is_response = (raw_command & response_flag) != 0;
        frame.payload = buffer.subspan(start + payload_offset, payload_size);

        consumed = start + frame_size;
        return DecodeResult::ok;
    }

    // Keep a trailing byte around: it may be the first half of a sync sequence
    // whose second half has not arrived yet.
    consumed = buffer.empty() ? 0 : buffer.size() - 1;
    return DecodeResult::no_sync;
}

ChannelStatus to_channel_status(const StatusResponse &response) {
    ChannelStatus status;
    status.state = static_cast<ChannelState>(response.state);
    status.error = static_cast<ChannelError>(response.error);
    status.filament_at_inlet = (response.flags & status_flag_filament_at_inlet) != 0;
    status.motor_running = (response.flags & status_flag_motor_running) != 0;
    status.motor_rpm = response.motor_rpm;
    status.wheel_rpm = response.wheel_rpm;
    status.moved_mm_x100 = response.moved_mm_x100;
    return status;
}

} // namespace buddy::autofeeder
