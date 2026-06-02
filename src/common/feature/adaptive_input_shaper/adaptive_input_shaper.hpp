#pragma once

#include <option/has_input_shaper_calibration.h>
#include <option/has_local_accelerometer.h>

namespace feature::adaptive_input_shaper {

#if HAS_INPUT_SHAPER_CALIBRATION() && HAS_LOCAL_ACCELEROMETER()
void step();
#else
inline void step() {}
#endif

} // namespace feature::adaptive_input_shaper
