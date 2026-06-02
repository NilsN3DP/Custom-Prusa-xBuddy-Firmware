#pragma once

#include <option/has_input_shaper_calibration.h>
#include <option/has_local_accelerometer.h>

#include <cstdint>

namespace feature::adaptive_input_shaper {

enum class Mode : uint8_t {
    off,
    monitor,
    correct,
};

struct Status {
    Mode mode = Mode::off;
    bool active = false;
    bool safety_brake = true;
    bool faulted = false;
    float x_frequency = 0.0f;
    float y_frequency = 0.0f;
    float x_peak = 0.0f;
    float y_peak = 0.0f;
    uint32_t updates = 0;
};

#if HAS_INPUT_SHAPER_CALIBRATION() && HAS_LOCAL_ACCELEROMETER()
void step();
Status get_status();
#else
inline void step() {}
inline Status get_status() {
    return {};
}
#endif

} // namespace feature::adaptive_input_shaper
