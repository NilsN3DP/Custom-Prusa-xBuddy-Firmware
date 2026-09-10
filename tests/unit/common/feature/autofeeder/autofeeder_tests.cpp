#include "fake_controller.hpp"

#include <feature/autofeeder/autofeeder_load.hpp>

#include <catch2/catch.hpp>

using namespace buddy::autofeeder;
using test::FakeController;

namespace {

std::vector<uint8_t> encode(Command command, uint8_t seq, std::span<const uint8_t> payload) {
    std::array<uint8_t, max_frame_size> buffer;
    const size_t size = encode_frame(command, seq, payload, buffer);
    return { buffer.begin(), buffer.begin() + size };
}

} // namespace

TEST_CASE("autofeeder protocol: crc is the documented CRC-16/CCITT-FALSE", "[autofeeder]") {
    // Reference vector for CRC-16/CCITT-FALSE.
    const std::array<uint8_t, 9> check = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    REQUIRE(crc16(check) == 0x29B1);
}

TEST_CASE("autofeeder protocol: encode/decode round trip", "[autofeeder]") {
    const FeedRequest request {
        .channel = 3,
        .direction = static_cast<uint8_t>(FeedDirection::backward),
        .duty = 200,
        .max_length_mm = 1100,
        .timeout_ds = 600,
    };
    const auto frame = encode(Command::feed, 42, { reinterpret_cast<const uint8_t *>(&request), sizeof(request) });

    REQUIRE(frame.size() == frame_overhead + sizeof(FeedRequest));

    DecodedFrame decoded;
    size_t consumed = 0;
    REQUIRE(find_frame(frame, decoded, consumed) == DecodeResult::ok);
    CHECK(consumed == frame.size());
    CHECK(decoded.command == Command::feed);
    CHECK(decoded.seq == 42);
    CHECK_FALSE(decoded.is_response);
    REQUIRE(decoded.payload.size() == sizeof(FeedRequest));

    FeedRequest round_tripped;
    std::memcpy(&round_tripped, decoded.payload.data(), sizeof(round_tripped));
    CHECK(round_tripped.channel == 3);
    CHECK(round_tripped.max_length_mm == 1100);
    CHECK(round_tripped.timeout_ds == 600);
}

TEST_CASE("autofeeder protocol: response flag is decoded separately", "[autofeeder]") {
    const auto frame = encode(static_cast<Command>(static_cast<uint8_t>(Command::info) | response_flag), 7, {});

    DecodedFrame decoded;
    size_t consumed = 0;
    REQUIRE(find_frame(frame, decoded, consumed) == DecodeResult::ok);
    CHECK(decoded.command == Command::info);
    CHECK(decoded.is_response);
}

TEST_CASE("autofeeder protocol: leading garbage is skipped", "[autofeeder]") {
    auto frame = encode(Command::info, 1, {});
    std::vector<uint8_t> buffer = { 0x00, 0xFF, 0xA5, 0x11 };
    buffer.insert(buffer.end(), frame.begin(), frame.end());

    DecodedFrame decoded;
    size_t consumed = 0;
    REQUIRE(find_frame(buffer, decoded, consumed) == DecodeResult::ok);
    CHECK(consumed == buffer.size());
    CHECK(decoded.command == Command::info);
}

TEST_CASE("autofeeder protocol: incomplete frame is not consumed", "[autofeeder]") {
    const auto frame = encode(Command::info, 1, {});
    const std::span<const uint8_t> partial(frame.data(), frame.size() - 1);

    DecodedFrame decoded;
    size_t consumed = 0;
    CHECK(find_frame(partial, decoded, consumed) == DecodeResult::incomplete);
    CHECK(consumed == 0);
}

TEST_CASE("autofeeder protocol: bad crc is reported and skipped", "[autofeeder]") {
    auto frame = encode(Command::info, 1, {});
    frame.back() ^= 0xFF;

    DecodedFrame decoded;
    size_t consumed = 0;
    CHECK(find_frame(frame, decoded, consumed) == DecodeResult::bad_crc);
    // The sync bytes are consumed so that the scan can make progress.
    CHECK(consumed == 2);
}

TEST_CASE("autofeeder protocol: an overlong length field does not read out of bounds", "[autofeeder]") {
    const std::vector<uint8_t> buffer = { sync_byte_0, sync_byte_1, 0x01, 0x00, 0xFF, 0x00, 0x00 };

    DecodedFrame decoded;
    size_t consumed = 0;
    CHECK(find_frame(buffer, decoded, consumed) == DecodeResult::bad_crc);
    CHECK(consumed == 2);
}

TEST_CASE("autofeeder driver: connects and reports controller info", "[autofeeder]") {
    FakeController controller;
    controller.channel_count = 4;
    controller.fw_major = 2;
    controller.fw_minor = 3;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 1000;
    test::run_until(feeder, now, [&] { return feeder.is_connected(); });

    REQUIRE(feeder.is_connected());
    CHECK(feeder.channel_count() == 4);
    CHECK(feeder.info().fw_major == 2);
    CHECK(feeder.info().fw_minor == 3);
}

TEST_CASE("autofeeder driver: refuses an incompatible protocol version", "[autofeeder]") {
    FakeController controller;
    controller.protocol_version_to_report = protocol_version + 1;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 0;
    for (int i = 0; i < 500; i++) {
        feeder.step(now);
        now += 5;
    }

    CHECK_FALSE(feeder.is_connected());
}

TEST_CASE("autofeeder driver: polls channel status", "[autofeeder]") {
    FakeController controller;
    controller.channels[1].filament_at_inlet = true;
    controller.channels[1].state = ChannelState::filament_at_inlet;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 0;
    test::run_until(feeder, now, [&] {
        return feeder.status(1).state == ChannelState::filament_at_inlet;
    });

    CHECK(feeder.status(1).filament_at_inlet);
}

TEST_CASE("autofeeder driver: recovers from a corrupted response", "[autofeeder]") {
    FakeController controller;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    controller.corrupt_next_response = true;

    uint32_t now = 0;
    test::run_until(feeder, now, [&] { return feeder.is_connected(); });

    // The first response was garbled; the retry got through.
    CHECK(feeder.is_connected());
}

TEST_CASE("autofeeder driver: a silent controller counts as disconnected", "[autofeeder]") {
    FakeController controller;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 0;
    test::run_until(feeder, now, [&] { return feeder.is_connected(); });
    REQUIRE(feeder.is_connected());

    controller.mute = true;

    for (int i = 0; i < 400; i++) {
        feeder.step(now);
        now += 5;
    }

    CHECK_FALSE(feeder.is_connected());
}

TEST_CASE("autofeeder driver: a stop supersedes a feed that was not sent yet", "[autofeeder]") {
    FakeController controller;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 0;
    test::run_until(feeder, now, [&] { return feeder.is_connected(); });

    REQUIRE(feeder.start_feed(0, FeedDirection::forward, duty_feed, 1000, 600));
    REQUIRE(feeder.request_stop(0));

    test::run_until(feeder, now, [&] { return !feeder.has_pending_request(0); });

    CHECK(controller.channels[0].feed_count == 0);
    CHECK(controller.channels[0].stop_count == 1);
}

TEST_CASE("autofeeder driver: rejects requests for channels the controller does not have", "[autofeeder]") {
    FakeController controller;
    controller.channel_count = 2;

    AutoFeeder feeder;
    feeder.set_transport(&controller);

    uint32_t now = 0;
    test::run_until(feeder, now, [&] { return feeder.is_connected(); });

    CHECK(feeder.start_feed(0, FeedDirection::forward, duty_feed, 1000, 600));
    CHECK_FALSE(feeder.start_feed(3, FeedDirection::forward, duty_feed, 1000, 600));
}

namespace {

/// Drives feeder and operation together, the way marlin_server and the pause
/// state machine do.
struct OperationHarness {
    FakeController controller;
    AutoFeeder feeder;
    FeedOperation operation { feeder };
    uint32_t now = 0;
    bool sensor_has_filament = false;

    OperationHarness() {
        feeder.set_transport(&controller);
        while (!feeder.is_connected()) {
            feeder.step(now);
            now += 5;
        }
    }

    FeedResult run(int max_steps = 10000) {
        for (int i = 0; i < max_steps; i++) {
            feeder.step(now);
            const FeedResult result = operation.step(now, sensor_has_filament);
            if (is_finished(result)) {
                return result;
            }
            now += 5;
        }
        return FeedResult::busy;
    }

    /// Runs for \p steps iterations without expecting the operation to end.
    void advance(int steps) {
        for (int i = 0; i < steps; i++) {
            feeder.step(now);
            operation.step(now, sensor_has_filament);
            now += 5;
        }
    }
};

} // namespace

TEST_CASE("autofeeder load: finishes when the filament reaches the side sensor", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));

    harness.advance(40);
    CHECK(harness.operation.is_busy());
    CHECK(harness.feeder.status(0).state == ChannelState::feeding);

    harness.sensor_has_filament = true;

    CHECK(harness.run() == FeedResult::finished);
    CHECK(harness.controller.channels[0].stop_count == 1);
}

TEST_CASE("autofeeder load: starts slowly and ramps the drive level up", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));

    harness.advance(10);
    CHECK(harness.controller.channels[0].last_duty == duty_engage);

    // Filament does not arrive; the drive level should climb towards the maximum.
    harness.advance(600);
    CHECK(harness.controller.channels[0].last_duty == duty_max);
}

TEST_CASE("autofeeder load: aborts when the filament leaves the inlet", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    harness.controller.channels[0].filament_at_inlet = false;

    CHECK(harness.run() == FeedResult::no_filament);
    CHECK(harness.controller.channels[0].stop_count == 1);
}

TEST_CASE("autofeeder load: reports a stalled motor", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    harness.controller.channels[0].state = ChannelState::failed;
    harness.controller.channels[0].error = ChannelError::motor_speed;

    CHECK(harness.run() == FeedResult::feeder_error);
    CHECK(harness.operation.error() == ChannelError::motor_speed);
}

TEST_CASE("autofeeder load: reports slipping filament", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    harness.controller.channels[0].state = ChannelState::failed;
    harness.controller.channels[0].error = ChannelError::wheel_speed;

    CHECK(harness.run() == FeedResult::feeder_error);
    CHECK(harness.operation.error() == ChannelError::wheel_speed);
}

TEST_CASE("autofeeder load: the full length without an arrival is not_arrived", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    // The controller ran out of length budget and stopped by itself.
    harness.controller.channels[0].state = ChannelState::stopped;

    CHECK(harness.run() == FeedResult::not_arrived);
}

TEST_CASE("autofeeder load: an unavailable channel does not start an operation", "[autofeeder]") {
    OperationHarness harness;

    CHECK_FALSE(harness.operation.start_load(4, harness.now));
    CHECK(harness.operation.result() == FeedResult::unavailable);
    CHECK_FALSE(harness.operation.is_busy());
}

TEST_CASE("autofeeder retract: pulls back past the sensor and then to the inlet", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;
    harness.sensor_has_filament = true;

    REQUIRE(harness.operation.start_retract(0, harness.now));
    harness.advance(20);

    CHECK(harness.controller.channels[0].last_direction == FeedDirection::backward);
    CHECK(harness.operation.is_busy());

    // Filament clears the side sensor; the feeder keeps pulling for the rest of
    // the tube and then stops on its own.
    harness.sensor_has_filament = false;
    harness.advance(20);
    const int feeds_after_clear = harness.controller.channels[0].feed_count;
    CHECK(feeds_after_clear == 2);

    harness.controller.channels[0].state = ChannelState::stopped;
    CHECK(harness.run() == FeedResult::finished);
}

TEST_CASE("autofeeder: losing the controller mid-move fails the operation", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    harness.controller.mute = true;

    CHECK(harness.run() == FeedResult::unavailable);
}

TEST_CASE("autofeeder: abort stops the motor", "[autofeeder]") {
    OperationHarness harness;
    harness.controller.channels[0].filament_at_inlet = true;

    REQUIRE(harness.operation.start_load(0, harness.now));
    harness.advance(20);

    harness.operation.abort();
    CHECK_FALSE(harness.operation.is_busy());

    harness.advance(20);
    CHECK(harness.controller.channels[0].stop_count == 1);
}
