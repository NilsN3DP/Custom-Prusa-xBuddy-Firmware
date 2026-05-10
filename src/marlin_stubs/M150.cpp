/**
 * @file
 */
#include "PrusaGcodeSuite.hpp"
#include "../../lib/Marlin/Marlin/src/gcode/parser.h"
#include "leds/status_leds_handler.hpp"
#if HAS_SIDE_LEDS()
    #include "leds/side_strip_handler.hpp"
#endif
#include <algorithm>
#include <optional>

namespace {

std::optional<leds::ColorRGBW> parse_color() {
    if (parser.seen("RGBW")) {
        const uint8_t R = parser.byteval('R', 0);
        const uint8_t G = parser.byteval('G', 0);
        const uint8_t B = parser.byteval('B', 0);
        const uint8_t W = parser.byteval('W', 0);
        return leds::ColorRGBW { R, G, B, W };

    } else if (parser.seen('S') && parser.seen('H') && parser.seen('V')) {
        float H = parser.floatval('H');
        float S = parser.floatval('S');
        float V = parser.floatval('V');
        return leds::ColorRGBW::from_hsv({ H, S, V });
    }
    return std::nullopt;
}

uint8_t percent_to_pwm(uint8_t percent) {
    return static_cast<uint8_t>(std::min<uint8_t>(percent, 100) * 255 / 100);
}

} // namespace

/**
 * @brief Set display led animations
 *
 * Color input supports RGB and HSV format
 *
 */

/**
 * \addtogroup G-Codes
 */
/**
 *### M150: Set LED color <a href="https://reprap.org/wiki/G-code#M150:_Set_LED_color">M150: Set LED color</a>
 *
 *#### Usage
 *
 *    M150 [ E | R | G | B | W | H | S | V | A | C | P ]
 *
 *#### Parameters
 *
 * - `E` - Enable RGB status bar: 0 = off, 1 = on
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
 * - `W` - White intensity from 0 to 255, if supported
 *
 * HSV color space
 * - `H` - Hue from 0 to 360
 * - `S` - Saturation from 0 to 100
 * - `V` - Saturation form 0 to 100
 *
 * Effect
 * - `A` - Animation type
 *   - `0` - Solid color
 *   - `1` - Pulsing
 * - `C` - Printer state
 *   - `0` - Idle
 *   - `1` - Printing
 *   - `2` - Pausing
 *   - `3` - Resuming
 *   - `4` - Aborting
 *   - `5` - Finishing
 *   - `6` - Warning
 *   - `7` - PowerPanic
 *
 * - `P` - Period in ms
 *
 * Backward compatibility: `M150 S<n>` still selects printer state when `H` and
 * `V` are not present.
 */
void PrusaGcodeSuite::M150() {
    if (parser.seen('E')) {
        leds::StatusLedsHandler::instance().set_active(parser.boolval('E'));
    }

    const bool legacy_state = parser.seen('S') && !parser.seen('H') && !parser.seen('V') && !parser.seen('A') && !parser.seen("RGBW");
    if (parser.seen('C') || legacy_state) {
        uint16_t state = parser.byteval(parser.seen('C') ? 'C' : 'S');
        if (state > static_cast<uint8_t>(leds::StateAnimation::_last)) {
            return;
        }
        leds::StatusLedsHandler::instance().set_animation(static_cast<leds::StateAnimation>(state));
    }

    if (parser.seen('A') || parser.seen("RGBW") || (parser.seen('H') && parser.seen('S') && parser.seen('V'))) {
        uint8_t animation = parser.byteval('A', static_cast<uint8_t>(leds::AnimationType::Solid));
        if (animation > static_cast<uint8_t>(leds::AnimationType::_last)) {
            return;
        }
        auto color = parse_color();
        if (color) {
            uint16_t period = parser.ushortval('P', 0);
            leds::StatusLedsHandler::instance().set_custom_animation(*color, static_cast<leds::AnimationType>(animation), period);
        }
    }
}

/**
 *### M151: Set LED strip <a href="https://reprap.org/wiki/G-code#M151:_Set_LED_strip">M151: Set LED strip</a>
 *
 * Controls chamber/side LEDs on printers that provide them.
 *
 *#### Usage
 *
 *    M151 [ E | Q | I | M | R | G | B | W | H | S | V | D | T ]
 *
 *#### Parameters
 *
 * - `E` - Enable chamber/side LEDs: 0 = off, 1 = on
 * - `Q` - Maximum brightness in percent, 0 to 100
 * - `I` - Dimmed brightness in percent, 0 to 100
 * - `M` - Dimming mode: 0 = never, 1 = always, 2 = on idle
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
 * - `W` - White intensity from 0 to 255, if supported
 *
 * HSV color space
 * - `H` - Hue from 0 to 360
 * - `S` - Saturation from 0 to 100
 * - `V` - Saturation form 0 to 100
 *
 * Effect
 * - `D` - duration in milliseconds, iX only: set to 0 for infinite duration
 * - `T` - transition in milliseconds (fade in / fade out)
 *
 * Fade in is counted toward duration,
 * so if duration is greater than 0 and less than transition,
 * effect doesn't reach full color intensity.
 * Fade out is not counted toward duration.
 */
#if HAS_SIDE_LEDS()
void PrusaGcodeSuite::M151() {
    auto &side_strip = leds::SideStripHandler::instance();

    if (parser.seen('E')) {
        side_strip.set_max_brightness(parser.boolval('E') ? 255 : 0);
    }

    if (parser.seen('Q')) {
        side_strip.set_max_brightness(percent_to_pwm(parser.byteval('Q')));
    }

    if (parser.seen('I')) {
        side_strip.set_dimmed_brightness(percent_to_pwm(parser.byteval('I')));
    }

    if (parser.seen('M')) {
        const uint8_t dimming_mode = parser.byteval('M');
        if (dimming_mode <= static_cast<uint8_t>(leds::DimmingEnabled::not_printing)) {
            side_strip.set_dimming_enabled(static_cast<leds::DimmingEnabled>(dimming_mode));
        }
    }

    auto color = parse_color();
    if (color) {
        auto color_val = color.value();
        uint32_t duration = parser.ulongval('D', 400);
        uint32_t transition = parser.ulongval('T', 100);
        side_strip.set_custom_color(color_val, duration, transition);
    }
}

/** @}*/

#endif
