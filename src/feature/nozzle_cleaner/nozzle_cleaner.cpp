#include "nozzle_cleaner.hpp"
#include "Marlin/src/gcode/gcode.h"
#include "raii/scope_guard.hpp"
#include <algorithm>
#include <config_store/store_instance.hpp>
#include <cstdio>
#include <gcode_loader.hpp>
#include <printers.h>

namespace nozzle_cleaner {

ConstexprString clean_sequence = "M106 S80\n" // fan on
                                 "G4 S2\n" // Wait for 2 seconds
                                 "G1 X267.4 Y284.75 F3000\n"
                                 "G1 X253.4 Y284.75 F3000\n"
                                 "G1 X267.4 Y284.75 F3000\n"
                                 "G1 X253.4 Y284.75 F3000\n"
                                 "G1 X253.4 Y305.0 F3000\n"
                                 "M106 S0\n" // fan off
                                 "G1 X254 Y285 F5000\n"
                                 "G1 X248 Y299 F5000\n"
                                 "G1 X235 Y285 F5000\n"
                                 "G1 X243 Y304 F5000\n"
                                 "G1 X230 Y291 F5000\n"
                                 "G1 X235 Y306 F5000\n"
                                 "G1 X224 Y296 F5000\n"
                                 "G1 X226 Y306 F3000\n"
                                 "G1 X248 Y288 F3000\n"
                                 "G1 X247 Y284 F3000\n"
                                 "G1 X229 Y306 F3000\n"
                                 "G1 X254 Y285 F5000\n"
                                 "G1 X248 Y299 F5000\n"
                                 "G1 X235 Y285 F5000\n"
                                 "G1 X243 Y304 F5000\n"
                                 "G1 X230 Y291 F5000\n"
                                 "G1 X235 Y306 F5000\n"
                                 "G1 X224 Y296 F5000\n"
                                 "G1 X226 Y306 F3000\n"
                                 "G1 X248 Y288 F3000\n"
                                 "G1 X247 Y284 F3000\n"
                                 "G1 X229 Y306 F3000";

ConstexprString vblade_cut_sequence = "M106 S200\n" // fan on
                                      "G4 S4\n" // Wait for 4 seconds
                                      "G1 X267.4 Y284.75 F3000\n"
                                      "G1 X253.4 Y284.75 F3000\n"
                                      "G1 X267.4 Y284.75 F3000\n"
                                      "G1 X253.4 Y284.75 F3000\n"
                                      "G1 X253.4 Y305.0 F3000\n"
                                      "M106 S0\n"; // fan off

ConstexprString clean_filename
    = "nozzle_cleaner_clean";
ConstexprString vblade_cut_filename = "nozzle_cleaner_vblade_cut";

namespace {

enum NozzleCleaningProfile : uint8_t {
    standard = 0,
    printed_wiper = 1,
    custom = 2,
};

ConstexprString printed_wiper_clean_filename = "nozzle_cleaner_printed_wiper";
ConstexprString custom_clean_filename = "nozzle_cleaner_custom";

#if PRINTER_IS_PRUSA_COREONEL()
ConstexprString printed_wiper_clean_sequence = "G0 Z3 F8000\n"
                                               "G0 Y0 F8000\n"
                                               "G0 X30 F8000\n"
                                               "G0 Y-7 F8000\n"
                                               "G0 Z1 F3000\n"
                                               "G0 X2.0\n"
                                               "G0 Y-6.3\n"
                                               "G0 X30.0\n"
                                               "G0 Y-5.0\n"
                                               "G0 X26.1\n"
                                               "G0 Y-7.0\n"
                                               "G0 X22.2\n"
                                               "G0 Y-5.0\n"
                                               "G0 X18.3\n"
                                               "G0 Y-7.0\n"
                                               "G0 X14.4\n"
                                               "G0 Y-5.0\n"
                                               "G0 X10.6\n"
                                               "G0 Y-7.0\n"
                                               "G0 X6.7\n"
                                               "G0 Y-5.0\n"
                                               "G0 X2.0\n"
                                               "G0 Y-5.0\n"
                                               "G0 Z3\n"
                                               "G0 Y0\n"
                                               "G0 X150\n";
#else
ConstexprString printed_wiper_clean_sequence = "G0 Z3 F8000\n"
                                               "G0 Y0 F8000\n"
                                               "G0 X210 F8000\n"
                                               "G0 Y-14 F8000\n"
                                               "G0 Z1.5 F3000\n"
                                               "G0 X168 Y-16 Z0.5 F3000\n"
                                               "G0 X168 Y-14 Z0.5 F3000\n"
                                               "G0 X210 Y-16 Z0.5 F3000\n"
                                               "G0 X168 Y-14 Z0.5 F3000\n"
                                               "G0 X168 Y-16 Z0.5 F3000\n"
                                               "G0 X210 Y-14 Z0.5 F3000\n"
                                               "G0 X210 Y-14 Z0.5 F3000\n"
                                               "G0 X168 Y-16 Z0.5 F3000\n"
                                               "G0 X168 Y-14 Z0.5 F3000\n"
                                               "G0 X210 Y-18 Z0.5 F3000\n"
                                               "G0 X168 Y-16 Z0.5 F3000\n"
                                               "G0 X168 Y-16 Z0.5 F3000\n"
                                               "G0 X210 Y-14 Z1.5 F3000\n"
                                               "G0 Z3 F8000\n"
                                               "G0 Y0 F8000\n"
                                               "G0 X150 F8000\n";
#endif

static uint16_t selected_profile_temperature() {
    switch (config_store().nozzle_cleaning_profile.get()) {
    case printed_wiper:
        return config_store().nozzle_cleaning_printed_wiper_temperature.get();
    case custom:
        return config_store().nozzle_cleaning_custom_temperature.get();
    case standard:
    default:
        return config_store().nozzle_cleaning_standard_temperature.get();
    }
}

const char *generated_brush_sequence(float start_x, float start_y, float end_x, float end_y) {
    static char buffer[1536];
    const uint8_t fan = std::min<uint8_t>(config_store().nozzle_cleaning_custom_fan.get(), 100);
    const uint8_t passes = std::clamp<uint8_t>(config_store().nozzle_cleaning_custom_passes.get(), 1, 10);
    const uint16_t speed = std::clamp<uint16_t>(config_store().nozzle_cleaning_custom_speed.get(), 500, 8000);

    int written = std::snprintf(buffer, sizeof(buffer),
        "M106 S%u\n"
        "G4 S1\n"
        "G0 Z3 F8000\n"
        "G0 Y0 F8000\n"
        "G0 X%.2f F8000\n"
        "G0 Y%.2f F8000\n"
        "G0 Z1 F3000\n",
        (unsigned)((fan * 255u + 50u) / 100u),
        (double)start_x, (double)start_y);

    for (uint8_t i = 0; i < passes && written > 0 && written < static_cast<int>(sizeof(buffer)); ++i) {
        written += std::snprintf(buffer + written, sizeof(buffer) - written,
            "G1 X%.2f Y%.2f F%u\n"
            "G1 X%.2f Y%.2f F%u\n",
            (double)end_x, (double)end_y, (unsigned)speed,
            (double)start_x, (double)start_y, (unsigned)speed);
    }

    if (written > 0 && written < static_cast<int>(sizeof(buffer))) {
        std::snprintf(buffer + written, sizeof(buffer) - written,
            "G0 Z3 F8000\n"
            "G0 Y0 F8000\n"
            "G0 X150 F8000\n"
            "M106 S0\n");
    }
    return buffer;
}

const char *selected_clean_sequence_without_temperature() {
    switch (config_store().nozzle_cleaning_profile.get()) {
    case printed_wiper:
        return printed_wiper_clean_sequence;
    case custom:
        return generated_brush_sequence(
            config_store().nozzle_cleaning_custom_start_x.get(),
            config_store().nozzle_cleaning_custom_start_y.get(),
            config_store().nozzle_cleaning_custom_end_x.get(),
            config_store().nozzle_cleaning_custom_end_y.get());
    case standard:
    default:
        return clean_sequence;
    }
}

} // namespace

static GCodeLoader &nozzle_cleaner_gcode_loader_instance() {
    static GCodeLoader nozzle_cleaner_gcode_loader;
    return nozzle_cleaner_gcode_loader;
}

const char *selected_clean_sequence() {
    const char *sequence = selected_clean_sequence_without_temperature();
    const uint16_t temperature = selected_profile_temperature();
    if (temperature == 0) {
        return sequence;
    }

    static char buffer[2048];
    std::snprintf(buffer, sizeof(buffer), "M109 S%u\n%s", static_cast<unsigned>(temperature), sequence);
    return buffer;
}

uint16_t selected_clean_temperature() {
    return selected_profile_temperature();
}

const char *selected_clean_filename() {
    switch (config_store().nozzle_cleaning_profile.get()) {
    case printed_wiper:
        return printed_wiper_clean_filename;
    case custom:
        return custom_clean_filename;
    case standard:
    default:
        return clean_filename;
    }
}

void load_clean_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(selected_clean_filename(), selected_clean_sequence());
}

void load_vblade_cut_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(vblade_cut_filename, vblade_cut_sequence);
}

bool is_loader_idle() {
    return nozzle_cleaner_gcode_loader_instance().is_idle();
}

bool is_loader_buffering() {
    return nozzle_cleaner_gcode_loader_instance().is_buffering();
}

bool execute() {
    // If we are idle or buffering there is no point in trying to execute but we dont want to reset if we are buffering so we just return false
    if (is_loader_idle() || is_loader_buffering()) {
        return false;
    }

    auto loader_result = nozzle_cleaner_gcode_loader_instance().get_result();
    ScopeGuard resetLoader = [&] { // Ensure the loader is always reset (the exception is if we are buffering or not idle, which is handled above)
        reset();
    };

    // this means the gcode was loaded successfully -> ready to execute it
    if (loader_result.has_value()) {
        GcodeSuite::process_subcommands_now(loader_result.value());
        return true;
    } else { // Here we have an error so we finished unsuccessfully and need to reset the loader for the next use
        return false;
    }
}

void reset() {
    nozzle_cleaner_gcode_loader_instance().reset();
}

} // namespace nozzle_cleaner
