/**
 * @file
 */
#include "PrusaGcodeSuite.hpp"
#include "../../lib/Marlin/Marlin/src/gcode/parser.h"
#include "leds/status_leds_handler.hpp"
#if HAS_SIDE_LEDS()
    #include "leds/side_strip_handler.hpp"
#endif
#include <option/has_xbuddy_extension.h>
#include <option/xbuddy_extension_variant.h>
#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
    #include <feature/xbuddy_extension/xbuddy_extension.hpp>
#endif
#include <algorithm>
#include <optional>

std::optional<leds::ColorRGBW> parse_color() {
    if (parser.seen('R') && parser.seen('G') && parser.seen('B')) {
        uint8_t R = parser.byteval('R');
        uint8_t G = parser.byteval('G');
        uint8_t B = parser.byteval('B');
        return leds::ColorRGBW { R, G, B };

    } else if (parser.seen('S') && parser.seen('H') && parser.seen('V')) {
        float H = parser.floatval('H');
        float S = parser.floatval('S');
        float V = parser.floatval('V');
        return leds::ColorRGBW::from_hsv({ H, S, V });
    }
    return std::nullopt;
}

#if HAS_SIDE_LEDS()
static leds::ColorRGBW side_strip_color(leds::ColorRGBW color) {
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    const uint8_t white = std::max(color.w, std::max(color.r, std::max(color.g, color.b)));
    return leds::ColorRGBW(0, 0, 0, white);
#else
    return color;
#endif
}
#endif

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
 *    M150 [ R | B | G | H | S | V | A | S | P ]
 *
 *#### Parameters
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
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
 * - `S` - Printer state
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
 */
void PrusaGcodeSuite::M150() {
    if (parser.seen('S')) {
        uint16_t state = parser.byteval('S');
        if (state > static_cast<uint8_t>(leds::StateAnimation::_last)) {
            return;
        }
        leds::StatusLedsHandler::instance().set_animation(static_cast<leds::StateAnimation>(state));
    } else if (parser.seen('A')) {
        uint8_t animation = parser.byteval('A');
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
 * Only XL and iX
 *
 *#### Usage
 *
 *    M151 [ R | B | G | H | S | V | D | T ]
 *
 *#### Parameters
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
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
    auto color = parse_color();
    if (color) {
        auto color_val = side_strip_color(color.value());
        uint32_t duration = parser.ulongval('D', 400);
        uint32_t transition = parser.ulongval('T', 100);
        leds::SideStripHandler::instance().set_custom_color(color_val, duration, transition);
    }
}

/** @}*/

#endif

/**
 *### M152: Set custom Prusa light target
 *
 *#### Usage
 *
 *    M152 L[target] [ R | B | G | H | S | V | A | P ]
 *
 *#### Parameters
 *
 * - `L` - Light target: 0 display/status LEDs, 1 bed LEDs, 2 chamber LEDs
 */
void PrusaGcodeSuite::M152() {
    const auto color = parse_color();
    if (!color) {
        return;
    }

    const uint8_t target = parser.byteval('L', 0);
    switch (target) {
    case 0: {
        const uint8_t animation = parser.byteval('A', static_cast<uint8_t>(leds::AnimationType::Solid));
        if (animation > static_cast<uint8_t>(leds::AnimationType::_last)) {
            return;
        }
        const uint16_t period = parser.ushortval('P', 0);
        leds::StatusLedsHandler::instance().set_custom_animation(*color, static_cast<leds::AnimationType>(animation), period);
        break;
    }

    case 1:
#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
        buddy::xbuddy_extension().set_bed_leds_color(*color);
#endif
        break;

    case 2:
#if HAS_SIDE_LEDS()
        leds::SideStripHandler::instance().set_custom_color(side_strip_color(*color), parser.ulongval('D', 300000), parser.ulongval('T', 100));
#endif
#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
        {
            const uint8_t white = std::max({ color->w, color->r, color->g, color->b });
            buddy::xbuddy_extension().set_chamber_leds_percent(buddy::XBuddyExtension::led_pwm2pct(white));
        }
#endif
        break;

    default:
        break;
    }
}

#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
/**
 *### M153: Set custom Prusa fan target
 *
 *#### Usage
 *
 *    M153 T[target] S[value]
 *
 *#### Parameters
 *
 * - `T` - Fan target: 0 chamber cooling fans, 1 filtration fan
 * - `S` - Target in percent. Use `-1` for Auto.
 */
void PrusaGcodeSuite::M153() {
    const uint8_t target = parser.byteval('T', 0);
    const int16_t speed = parser.intval('S', -2);
    if (speed < -1 || speed > 100) {
        return;
    }

    const auto pwm_target = speed < 0 ? buddy::XBuddyExtension::FanPWMOrAuto(pwm_auto) : buddy::XBuddyExtension::FanPWM::from_percent(speed);

    switch (target) {
    case 0:
        buddy::xbuddy_extension().set_fan_target_pwm(buddy::XBuddyExtension::Fan::cooling_fan_1, pwm_target);
        break;

    case 1:
        buddy::xbuddy_extension().set_fan_target_pwm(buddy::XBuddyExtension::Fan::filtration_fan, pwm_target);
        break;

    default:
        break;
    }
}
#endif
