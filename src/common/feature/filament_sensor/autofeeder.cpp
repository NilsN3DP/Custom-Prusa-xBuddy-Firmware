#include "autofeeder.hpp"
#include "filament_sensors_handler.hpp"

#include "core/serial.h"
#include "inc/MarlinConfig.h"
#include "module/stepper/indirection.h"

#include <config_store/store_instance.hpp>
#include <marlin_client.hpp>
#include <marlin_server.hpp>
#include <option/has_emergency_stop.h>

#if HAS_EMERGENCY_STOP()
    #include <feature/emergency_stop/emergency_stop.hpp>
#endif

#if ENABLED(COREONE_AUTOFEEDER)
    #if !PIN_EXISTS(Z_STEP) || !PIN_EXISTS(Z_DIR) || !PIN_EXISTS(Z_ENABLE)
        #error "COREONE_AUTOFEEDER needs Z_STEP, Z_DIR and Z_ENABLE pins for the free ZL stepper port."
    #endif

    #if AXIS_IS_TMC(Z)
        #include "feature/tmc_util.h"
    #endif
#endif

namespace buddy::autofeeder {

#if ENABLED(COREONE_AUTOFEEDER)

namespace {

enum class Mode {
    idle,
    automatic,
    manual,
    calibration,
};

Mode mode = Mode::idle;
millis_t timeout_at_ms = 0;
uint32_t next_step_us = 0;
uint32_t step_interval_us = 0;
uint32_t emitted_steps = 0;
bool current_saved = false;
uint16_t saved_current_ma = 0;
bool timeout_latched = false;
bool calibration_error_latched = false;
float calibration_expected_mm = 0.0f;
float calibration_max_deviation_mm = 0.0f;
float calibration_max_deviation_pct = 0.0f;

bool extruder_has_filament() {
    return FSensors_instance().sensor_state(LogicalFilamentSensor::extruder) == FilamentSensorState::HasFilament;
}

bool extruder_has_no_filament() {
    return FSensors_instance().sensor_state(LogicalFilamentSensor::extruder) == FilamentSensorState::NoFilament;
}

bool side_has_filament() {
    return FSensors_instance().sensor_state(LogicalFilamentSensor::side) == FilamentSensorState::HasFilament;
}

bool unsafe_to_move() {
    return marlin_client::is_printing()
        || marlin_client::is_paused()
        || marlin_server::aborting_or_aborted()
        || marlin_server::printer_paused_extended()
#if HAS_EMERGENCY_STOP()
        || buddy::emergency_stop().in_emergency()
#endif
        ;
}

void restore_current() {
#if AXIS_IS_TMC(Z)
    if (current_saved) {
        stepperZ.rms_current(saved_current_ma);
        current_saved = false;
    }
#endif
}

void configure_current() {
#if AXIS_IS_TMC(Z)
    if (!current_saved) {
        saved_current_ma = stepperZ.rms_current();
        current_saved = true;
    }
    stepperZ.rms_current(COREONE_AUTOFEEDER_CURRENT);
#endif
}

void start(Mode new_mode) {
    if (mode != Mode::idle || extruder_has_filament() || unsafe_to_move() || calibration_error_latched) {
        return;
    }

    const float steps_per_second = COREONE_AUTOFEEDER_STEPS_PER_MM * COREONE_AUTOFEEDER_FEEDRATE_MM_S;
    if (steps_per_second <= 0.0f) {
        SERIAL_ERROR_MSG("Autofeeder: invalid steps/mm or feedrate");
        return;
    }

    // Use only the free ZL stepper signals directly; do not call enable_Z(), which may enable multiple Z drivers.
    const bool forward_dir = !INVERT_Z_DIR;
    Z_DIR_WRITE(COREONE_AUTOFEEDER_INVERT_DIR ? !forward_dir : forward_dir);
    Z_STEP_WRITE(LOW);
    configure_current();
    Z_ENABLE_WRITE(Z_ENABLE_ON);

    mode = new_mode;
    timeout_at_ms = millis() + COREONE_AUTOFEEDER_TIMEOUT_MS;
    step_interval_us = static_cast<uint32_t>(1000000.0f / steps_per_second);
    if (step_interval_us == 0) {
        step_interval_us = 1;
    }
    next_step_us = micros();
    emitted_steps = 0;
    timeout_latched = false;
}

void finish_calibration() {
    const float measured_mm = static_cast<float>(emitted_steps) / COREONE_AUTOFEEDER_STEPS_PER_MM;
    float allowed_mm = calibration_max_deviation_mm;
    if (calibration_max_deviation_pct > 0.0f) {
        const float allowed_pct_mm = calibration_expected_mm * calibration_max_deviation_pct * 0.01f;
        if (allowed_pct_mm > allowed_mm) {
            allowed_mm = allowed_pct_mm;
        }
    }

    const float deviation_mm = measured_mm > calibration_expected_mm
        ? measured_mm - calibration_expected_mm
        : calibration_expected_mm - measured_mm;
    const float suggested_steps_per_mm = calibration_expected_mm > 0.0f
        ? static_cast<float>(emitted_steps) / calibration_expected_mm
        : 0.0f;

    SERIAL_ECHOLNPAIR("Autofeeder calibration measured mm: ", measured_mm);
    SERIAL_ECHOLNPAIR("Autofeeder calibration expected mm: ", calibration_expected_mm);
    SERIAL_ECHOLNPAIR("Autofeeder calibration deviation mm: ", deviation_mm);
    SERIAL_ECHOLNPAIR("Autofeeder suggested steps/mm: ", suggested_steps_per_mm);

    if (deviation_mm > allowed_mm) {
        calibration_error_latched = true;
        SERIAL_ERROR_MSG("Autofeeder calibration failed / measured path deviates too much");
    } else {
        calibration_error_latched = false;
        SERIAL_ECHO_MSG("Autofeeder calibration ok");
    }
}

void step_motor() {
    const uint32_t now_us = micros();
    uint8_t emitted = 0;

    while (static_cast<int32_t>(now_us - next_step_us) >= 0 && emitted < COREONE_AUTOFEEDER_MAX_STEP_BURST) {
        Z_STEP_WRITE(HIGH);
        // Keep the spare Z stepper pulse high long enough without depending on board-specific delay macros.
        const uint32_t pulse_start_us = micros();
        while (static_cast<uint32_t>(micros() - pulse_start_us) < COREONE_AUTOFEEDER_STEP_PULSE_US) {
        }
        Z_STEP_WRITE(LOW);
        next_step_us += step_interval_us;
        ++emitted_steps;
        ++emitted;
    }

    if (emitted == COREONE_AUTOFEEDER_MAX_STEP_BURST) {
        next_step_us = micros() + step_interval_us;
    }
}

const char *state_name(FilamentSensorState state) {
    switch (state) {
    case FilamentSensorState::NoFilament:
        return "no_filament";
    case FilamentSensorState::HasFilament:
        return "has_filament";
    case FilamentSensorState::NotInitialized:
        return "not_initialized";
    case FilamentSensorState::NotCalibrated:
        return "not_calibrated";
    case FilamentSensorState::NotConnected:
        return "not_connected";
    case FilamentSensorState::Disabled:
        return "disabled";
    default:
        return "unknown";
    }
}

} // namespace

void cycle() {
    if (mode != Mode::idle) {
        if (unsafe_to_move()) {
            stop();
            return;
        }

        if (extruder_has_filament()) {
            if (mode == Mode::calibration) {
                finish_calibration();
            }
            stop();
            return;
        }

        if (ELAPSED(millis(), timeout_at_ms)) {
            timeout_latched = true;
            if (mode == Mode::calibration) {
                calibration_error_latched = true;
            }
            stop();
            SERIAL_ERROR_MSG("Autofeeder timeout / filament not detected at Nextruder");
            return;
        }

        step_motor();
        return;
    }

    if (side_has_filament()
        && extruder_has_no_filament()
        && config_store().fs_autoload_enabled.get()
        && !FSensors_instance().IsAutoloadLocked()
        && !unsafe_to_move()) {
        start(Mode::automatic);
    }
}

bool start_manual() {
    start(Mode::manual);
    return mode == Mode::manual;
}

bool start_calibration(float expected_distance_mm, float max_deviation_mm, float max_deviation_pct) {
    if (expected_distance_mm <= 0.0f) {
        SERIAL_ERROR_MSG("M8600: Calibration needs expected distance in L");
        return false;
    }

    calibration_expected_mm = expected_distance_mm;
    calibration_max_deviation_mm = max_deviation_mm;
    calibration_max_deviation_pct = max_deviation_pct;
    calibration_error_latched = false;
    start(Mode::calibration);
    return mode == Mode::calibration;
}

void stop() {
    if (mode == Mode::idle) {
        return;
    }

    Z_STEP_WRITE(LOW);
    Z_ENABLE_WRITE(!Z_ENABLE_ON);
    restore_current();
    mode = Mode::idle;
}

bool is_running() {
    return mode != Mode::idle;
}

void reset_error() {
    timeout_latched = false;
    calibration_error_latched = false;
}

void report_status() {
    SERIAL_ECHOLNPAIR("Autofeeder running: ", is_running() ? "yes" : "no");
    SERIAL_ECHOLNPAIR("Autofeeder last timeout: ", timeout_latched ? "yes" : "no");
    SERIAL_ECHOLNPAIR("Autofeeder calibration error: ", calibration_error_latched ? "yes" : "no");
    SERIAL_ECHOLNPAIR("Autofeeder emitted steps: ", emitted_steps);
    SERIAL_ECHOLNPAIR("Autofeeder side sensor: ", state_name(FSensors_instance().sensor_state(LogicalFilamentSensor::side)));
    SERIAL_ECHOLNPAIR("Autofeeder Nextruder sensor: ", state_name(FSensors_instance().sensor_state(LogicalFilamentSensor::extruder)));
}

#else

void cycle() {}
bool start_manual() { return false; }
bool start_calibration(float, float, float) { return false; }
void stop() {}
bool is_running() { return false; }
void reset_error() {}
void report_status() { SERIAL_ECHO_MSG("Autofeeder disabled"); }

#endif

} // namespace buddy::autofeeder
