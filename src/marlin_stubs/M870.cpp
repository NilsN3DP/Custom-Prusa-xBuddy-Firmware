#include "PrusaGcodeSuite.hpp"

#include "feature/automatic_chamber_vents/automatic_chamber_vents.hpp"
#include <Marlin/src/core/serial.h>
#include <Marlin/src/gcode/gcode.h>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M870: Control chamber vents.
 *
 * Open or close the vent grille.
 * This does not automatically retract before moving the head.
 *
 *#### Parameters
 * - `O` - Open intake
 * - `C` - Close intake
 * - `P` - Move to vent calibration point: 0 open start, 1 open end, 2 close start, 3 close end
 */
void PrusaGcodeSuite::M870() {
    const bool open = parser.seen('O');
    const bool close = parser.seen('C');
    const bool calibration_point = parser.seenval('P');

    if ((open && close) || (calibration_point && (open || close))) {
        SERIAL_ERROR_MSG("M870: Cannot combine O, C and P");
    } else if (open) {
        if (!automatic_chamber_vents::open()) {
            SERIAL_ERROR_MSG("M870: Failed to open chamber vents");
        }
    } else if (close) {
        if (!automatic_chamber_vents::close()) {
            SERIAL_ERROR_MSG("M870: Failed to close chamber vents");
        }
    } else if (calibration_point) {
        if (!automatic_chamber_vents::move_to_calibration_point(parser.byteval('P'))) {
            SERIAL_ERROR_MSG("M870: Failed to move to vent calibration point");
        }
    }
}

/** @}*/
