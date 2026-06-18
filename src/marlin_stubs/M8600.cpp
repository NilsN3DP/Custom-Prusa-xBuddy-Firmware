#include "PrusaGcodeSuite.hpp"

#include "core/serial.h"
#include "gcode/parser.h"
#include "inc/MarlinConfig.h"

#include <feature/filament_sensor/autofeeder.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M8600: CORE One auxiliary filament autofeeder
 *
 *#### Usage
 *
 *    M8600 S1 ; start manual autofeeder test
 *    M8600 S0 ; stop autofeeder
 *    M8600 T  ; report sensor and autofeeder status
 *    M8600 C L150 [D10|P20] ; calibrate sensor-to-Nextruder distance
 *    M8600 R  ; reset latched autofeeder errors
 */
void PrusaGcodeSuite::M8600() {
    if (parser.seen('T')) {
        buddy::autofeeder::report_status();
        return;
    }

    if (parser.seen('R')) {
        buddy::autofeeder::reset_error();
        SERIAL_ECHO_MSG("M8600: Autofeeder errors reset");
        return;
    }

    if (parser.seen('C')) {
#if ENABLED(COREONE_AUTOFEEDER)
        const float expected_distance_mm = parser.floatval('L', COREONE_AUTOFEEDER_CALIBRATION_DEFAULT_DISTANCE_MM);
        const float max_deviation_mm = parser.floatval('D', COREONE_AUTOFEEDER_CALIBRATION_MAX_DEVIATION_MM);
        const float max_deviation_pct = parser.floatval('P', COREONE_AUTOFEEDER_CALIBRATION_MAX_DEVIATION_PCT);

        if (!buddy::autofeeder::start_calibration(expected_distance_mm, max_deviation_mm, max_deviation_pct)) {
            SERIAL_ERROR_MSG("M8600: Autofeeder calibration could not start");
        }
#else
        SERIAL_ERROR_MSG("M8600: Autofeeder disabled");
#endif
        return;
    }

    if (parser.seen('S')) {
        if (parser.value_bool()) {
            if (!buddy::autofeeder::start_manual()) {
                SERIAL_ERROR_MSG("M8600: Autofeeder could not start");
            }
        } else {
            buddy::autofeeder::stop();
        }
        return;
    }

    buddy::autofeeder::report_status();
}

/** @}*/
