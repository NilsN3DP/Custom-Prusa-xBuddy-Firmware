#pragma once
#include <utils/led_color.hpp>

namespace leds {

namespace AcControllerLedsHandler {
    enum class CustomEffect : uint8_t {
        off = 0,
        static_color = 1,
        progress_percent = 2,
    };

    void set_custom_effect(CustomEffect effect, ColorRGBW color, uint8_t progress_percent, uint32_t duration_ms);
    void update(ColorRGBW &color, uint8_t progress_percent); // Call periodically to update LED state
}

} // namespace leds
