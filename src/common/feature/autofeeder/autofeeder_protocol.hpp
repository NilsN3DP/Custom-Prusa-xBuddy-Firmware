/// @file
/// Wire protocol between the printer and the external autofeeder controller.
///
/// The protocol is intentionally small and self-contained: this header and its
/// .cpp are meant to be copied verbatim into the feeder controller's firmware so
/// that both sides are guaranteed to agree on the framing.
///
/// Frame layout (all multi-byte fields little endian):
///
///     +------+------+------+------+--------+-----------------+---------+
///     | 0xA5 | 0x5A | cmd  | seq  | length | payload[length] | crc16   |
///     +------+------+------+------+--------+-----------------+---------+
///        0      1      2      3       4         5 ...          last 2
///
/// The CRC is a CRC-16/CCITT-FALSE over bytes 2 .. (5 + length - 1), i.e. it
/// covers everything but the sync bytes and the CRC itself.
///
/// A response repeats the \p cmd of the request with \p response_flag set and
/// the \p seq of the request. \p Command::error is sent instead when the request
/// could not be carried out at all.

#pragma once

#include "autofeeder_types.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace buddy::autofeeder {

// The structs below are memcpy'd to and from the wire, so both ends have to be
// little endian. Both the printer (STM32) and the feeder controller are.
static_assert(std::endian::native == std::endian::little, "autofeeder protocol assumes a little endian host");

/// Version of this protocol. Bumped on any incompatible change.
inline constexpr uint8_t protocol_version = 1;

inline constexpr uint8_t sync_byte_0 = 0xA5;
inline constexpr uint8_t sync_byte_1 = 0x5A;

/// Set in the command byte of a response.
inline constexpr uint8_t response_flag = 0x80;

/// Number of bytes a frame uses on top of its payload.
inline constexpr size_t frame_overhead = 7;

/// Largest payload any message of this protocol carries.
inline constexpr size_t max_payload_size = 32;

/// Largest possible frame.
inline constexpr size_t max_frame_size = frame_overhead + max_payload_size;

enum class Command : uint8_t {
    /// No payload. Response: \p InfoResponse.
    /// Also used as a keepalive/lost-connection probe.
    info = 0x01,

    /// Payload: channel index (1 byte). Response: \p StatusResponse.
    get_status = 0x02,

    /// Payload: \p FeedRequest. Response: \p StatusResponse.
    feed = 0x03,

    /// Payload: channel index (1 byte). Response: \p StatusResponse.
    stop = 0x04,

    /// Payload: \p SetLedRequest. Response: empty.
    set_led = 0x05,

    /// Response only. Payload: 1 byte \p ProtocolError.
    error = 0x7F,
};

/// Reported by the controller when a request could not be processed.
enum class ProtocolError : uint8_t {
    /// Command byte not recognised.
    unknown_command = 1,

    /// Payload length does not match the command.
    bad_payload = 2,

    /// Channel index out of range for this controller.
    bad_channel = 3,

    /// The controller is not in a state to accept this command.
    busy = 4,
};

#pragma pack(push, 1)

/// Payload of \p Command::info responses.
struct InfoResponse {
    uint8_t protocol_version;
    uint8_t channel_count;
    uint8_t fw_major;
    uint8_t fw_minor;
};
static_assert(sizeof(InfoResponse) == 4);

/// Payload of \p Command::feed requests.
struct FeedRequest {
    uint8_t channel;

    /// \p FeedDirection
    uint8_t direction;

    /// Motor drive level, 0..255 mapped to 0..100 % PWM duty.
    uint8_t duty;

    /// Movement is stopped after this many millimetres of filament.
    /// The controller measures this with the pinch wheel tachometers.
    uint16_t max_length_mm;

    /// Movement is stopped after this many tenths of a second, whatever the
    /// measured length. Acts as the controller-side watchdog: even if the
    /// printer stops talking, the motor will not run forever.
    uint16_t timeout_ds;
};
static_assert(sizeof(FeedRequest) == 7);

/// Payload of \p Command::set_led requests.
struct SetLedRequest {
    uint8_t channel;

    /// Brightness of the red status LED, 0..255.
    uint8_t red;

    /// Brightness of the white status LED, 0..255.
    uint8_t white;
};
static_assert(sizeof(SetLedRequest) == 3);

/// Payload of \p Command::get_status, \p Command::feed and \p Command::stop
/// responses.
struct StatusResponse {
    uint8_t channel;

    /// \p ChannelState
    uint8_t state;

    /// \p ChannelError
    uint8_t error;

    /// bit 0: filament present in the inlet port
    /// bit 1: motor is being driven
    uint8_t flags;

    uint16_t motor_rpm;
    uint16_t wheel_rpm;
    int32_t moved_mm_x100;
};
static_assert(sizeof(StatusResponse) == 12);

#pragma pack(pop)

inline constexpr uint8_t status_flag_filament_at_inlet = 1 << 0;
inline constexpr uint8_t status_flag_motor_running = 1 << 1;

/// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor).
uint16_t crc16(std::span<const uint8_t> data);

/// Builds a frame for \p command with \p payload into \p buffer.
///
/// \returns the number of bytes written, or 0 if \p buffer is too small or the
///          payload exceeds \p max_payload_size.
size_t encode_frame(Command command, uint8_t seq, std::span<const uint8_t> payload, std::span<uint8_t> buffer);

/// One decoded frame, pointing into the buffer that was decoded.
struct DecodedFrame {
    Command command;
    uint8_t seq;
    bool is_response;
    std::span<const uint8_t> payload;
};

/// Result of \p find_frame.
enum class DecodeResult : uint8_t {
    /// A complete, CRC-checked frame was found.
    ok,

    /// No sync sequence in the buffer at all; everything may be discarded.
    no_sync,

    /// A frame start was found but the buffer does not hold all of it yet.
    incomplete,

    /// A frame was found but its CRC did not match; it has been skipped.
    bad_crc,
};

/// Scans \p buffer for the next complete frame.
///
/// \param buffer     bytes received so far
/// \param frame      set to the decoded frame on \p DecodeResult::ok
/// \param consumed   set to the number of leading bytes of \p buffer that may be
///                   discarded. On \p DecodeResult::ok this includes the frame
///                   itself, on \p DecodeResult::bad_crc the damaged frame's sync
///                   bytes, and on \p DecodeResult::incomplete the garbage before
///                   the frame start.
DecodeResult find_frame(std::span<const uint8_t> buffer, DecodedFrame &frame, size_t &consumed);

/// Converts a \p StatusResponse payload into a \p ChannelStatus.
ChannelStatus to_channel_status(const StatusResponse &response);

} // namespace buddy::autofeeder
