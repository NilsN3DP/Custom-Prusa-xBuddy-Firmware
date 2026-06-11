/// @file
#include "automatic_chamber_vents.hpp"

#include <feature/print_status_message/print_status_message_guard.hpp>
#include <mapi/parking.hpp>
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <config_store/store_instance.hpp>
#include <printers.h>

#include <feature/chamber/chamber.hpp>

namespace automatic_chamber_vents {
namespace {

    /// The following constants define key positions for controlling a vent lever.
    /// These coordinates are absolute in the printer's coordinate system.
#if PRINTER_IS_PRUSA_COREONE()
    static constexpr auto Y_SAFE = -3.f; ///< Safe Y line with no risk of coming in contact with lever
    static constexpr auto Y_LEVER = -18.f; ///< In line with the lever
    static constexpr auto X_OPEN_START_POS = 37.f; ///< An X-axis position to the right of the lever, where opening move starts.
    static constexpr auto X_OPEN_END_POS = 24.f; ///< The X-axis position to move to on Y_LEVER to open the vents.
    static constexpr auto X_CLOSE_START_POS = 11.f; ///< An X-axis position to the left of the lever, where closing move starts.
    static constexpr auto X_CLOSE_END_POS = 26.f; ///< The X-axis position to move to on Y_LEVER to close the vents.
    static constexpr auto X_LEVER_MOVE_AWAY = 4.f; ///< The X-axis distance to move away from the lever after switch
    static constexpr auto lever_move_feedrate = feedRate_t(40.0f);
    static constexpr auto N3DP_Y_SAFE = 10.f;
    static constexpr auto N3DP_Y_LEVER = -7.f;
    static constexpr auto N3DP_X_OPEN_START_POS = 25.f;
    static constexpr auto N3DP_X_OPEN_END_POS = 42.f;
    static constexpr auto N3DP_X_CLOSE_START_POS = 50.f;
    static constexpr auto N3DP_X_CLOSE_END_POS = 35.f;
    static constexpr auto N3DP_X_LEVER_MOVE_AWAY = -4.f;
    static constexpr auto n3dp_lever_move_feedrate = feedRate_t(17.0f);
#elif PRINTER_IS_PRUSA_COREONEL()
    static constexpr auto Y_SAFE = 10.f; ///< Safe Y line with no risk of coming in contact with lever
    static constexpr auto Y_LEVER = -7.f; ///< In line with the lever
    static constexpr auto X_OPEN_START_POS = 25.f; ///< An X-axis position to the left of the lever, where opening move starts.
    static constexpr auto X_OPEN_END_POS = 42.f; ///< The X-axis position to move to on Y_LEVER to open the vents.
    static constexpr auto X_CLOSE_START_POS = 50.f; ///< An X-axis position to the right of the lever, where closing move starts.
    static constexpr auto X_CLOSE_END_POS = 35.f; ///< The X-axis position to move to on Y_LEVER to close the vents.
    static constexpr auto X_LEVER_MOVE_AWAY = -4.f; ///< The X-axis distance to move away from the lever after switch (COREONEL has inverted open/close direction)
    static constexpr auto lever_move_feedrate = feedRate_t(17.0f);
    static constexpr auto N3DP_Y_SAFE = Y_SAFE;
    static constexpr auto N3DP_Y_LEVER = Y_LEVER;
    static constexpr auto N3DP_X_OPEN_START_POS = X_OPEN_START_POS;
    static constexpr auto N3DP_X_OPEN_END_POS = X_OPEN_END_POS;
    static constexpr auto N3DP_X_CLOSE_START_POS = X_CLOSE_START_POS;
    static constexpr auto N3DP_X_CLOSE_END_POS = X_CLOSE_END_POS;
    static constexpr auto N3DP_X_LEVER_MOVE_AWAY = X_LEVER_MOVE_AWAY;
    static constexpr auto n3dp_lever_move_feedrate = lever_move_feedrate;
#else
    #error
#endif

    struct VentCoordinates {
        float x_open_start;
        float y_open_start;
        float x_open_end;
        float y_open_end;
        float x_close_start;
        float y_close_start;
        float x_close_end;
        float y_close_end;
        float y_travel_safe;
        float x_lever_move_away;
        feedRate_t feedrate;
    };

    enum TopVentPartsProfile : uint8_t {
        original_parts = 0,
        modded_n3dp_parts = 1,
        custom_vent_position = 2,
    };

    enum VentCalibrationPoint : uint8_t {
        open_start = 0,
        open_end = 1,
        close_start = 2,
        close_end = 3,
    };

    VentCoordinates original_coordinates() {
        return {
            .x_open_start = X_OPEN_START_POS,
            .y_open_start = Y_SAFE,
            .x_open_end = X_OPEN_END_POS,
            .y_open_end = Y_LEVER,
            .x_close_start = X_CLOSE_START_POS,
            .y_close_start = Y_SAFE,
            .x_close_end = X_CLOSE_END_POS,
            .y_close_end = Y_LEVER,
            .y_travel_safe = Y_SAFE,
            .x_lever_move_away = X_LEVER_MOVE_AWAY,
            .feedrate = lever_move_feedrate,
        };
    }

    VentCoordinates n3dp_coordinates() {
        return {
            .x_open_start = config_store().top_vent_n3dp_open_start_x.get(),
            .y_open_start = config_store().top_vent_n3dp_open_start_y.get(),
            .x_open_end = config_store().top_vent_n3dp_open_end_x.get(),
            .y_open_end = config_store().top_vent_n3dp_open_end_y.get(),
            .x_close_start = config_store().top_vent_n3dp_close_start_x.get(),
            .y_close_start = config_store().top_vent_n3dp_close_start_y.get(),
            .x_close_end = config_store().top_vent_n3dp_close_end_x.get(),
            .y_close_end = config_store().top_vent_n3dp_close_end_y.get(),
            .y_travel_safe = config_store().top_vent_n3dp_safe_y.get(),
            .x_lever_move_away = N3DP_X_LEVER_MOVE_AWAY,
            .feedrate = n3dp_lever_move_feedrate,
        };
    }

    VentCoordinates custom_coordinates() {
        const float x_open_start = config_store().top_vent_custom_open_start_x.get();
        const float x_open_end = config_store().top_vent_custom_open_end_x.get();

        return {
            .x_open_start = x_open_start,
            .y_open_start = config_store().top_vent_custom_open_start_y.get(),
            .x_open_end = x_open_end,
            .y_open_end = config_store().top_vent_custom_open_end_y.get(),
            .x_close_start = config_store().top_vent_custom_close_start_x.get(),
            .y_close_start = config_store().top_vent_custom_close_start_y.get(),
            .x_close_end = config_store().top_vent_custom_close_end_x.get(),
            .y_close_end = config_store().top_vent_custom_close_end_y.get(),
            .y_travel_safe = config_store().top_vent_custom_safe_y.get(),
            .x_lever_move_away = x_open_end > x_open_start ? -4.f : 4.f,
            .feedrate = feedRate_t(17.0f),
        };
    }

    VentCoordinates selected_coordinates() {
        switch (config_store().top_vent_parts_profile.get()) {
        case modded_n3dp_parts:
            return n3dp_coordinates();
        case custom_vent_position:
            return custom_coordinates();
        case original_parts:
        default:
            return original_coordinates();
        }
    }

    enum class VentState {
        open,
        close
    };

    /// @brief Plans a move to a new X-axis coordinate.
    /// @param x The target X-axis position.
    /// @param feedrate The speed of the move in mm/s.
    void plan_to_x(float x, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
        xyze_pos_t xyz = current_position;
        xyz.x = x;
        prepare_move_to(xyz, feedrate, {});
    }

    /// @brief Plans a move to a new Y-axis coordinate.
    /// @param y The target Y-axis position.
    /// @param feedrate The speed of the move in mm/s.
    void plan_to_y(float y, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
        xyze_pos_t xyz = current_position;
        xyz.y = y;
        prepare_move_to(xyz, feedrate, {});
    }

    /// @brief Prepares the printer for a vent lever switch.
    /// @return true on success, false on failure.
    bool before() {
        if (!GcodeSuite::G28_no_parser(true, true, false, { .only_if_needed = true, .precise = false })) {
            return false;
        }
        return true;
    }

    void after() {
        // Return to the home position after the vent operation.
        mapi::park(mapi::ZAction::no_move, mapi::park_positions[mapi::ParkPosition::park]);
    }

    void switch_lever(VentState wanted_state) {
        const VentCoordinates coordinates = selected_coordinates();

        const float start_x = wanted_state == VentState::open ? coordinates.x_open_start : coordinates.x_close_start;
        const float start_y = wanted_state == VentState::open ? coordinates.y_open_start : coordinates.y_close_start;
        const float end_x = wanted_state == VentState::open ? coordinates.x_open_end : coordinates.x_close_end;
        const float end_y = wanted_state == VentState::open ? coordinates.y_open_end : coordinates.y_close_end;

        plan_to_y(coordinates.y_travel_safe);
        plan_to_x(start_x);
        plan_to_y(start_y);
        if (start_y != end_y) {
            plan_to_y(end_y);
        }
        plan_to_x(end_x, coordinates.feedrate);
        // Back out before any side move, so the Bowden tube clears the printed vent part.
        plan_to_y(coordinates.y_travel_safe);
        // Move horizontally only on the safe Y line to release the lever tension.
        plan_to_x(wanted_state == VentState::open ? end_x + coordinates.x_lever_move_away : end_x - coordinates.x_lever_move_away, coordinates.feedrate);
    }

    void plan_to_calibration_point(const VentCoordinates &coordinates, VentCalibrationPoint point) {
        const auto plan_to_point = [&](float x, float y, feedRate_t feedrate = feedRate_t(XY_PROBE_FEEDRATE_MM_S)) {
            plan_to_y(coordinates.y_travel_safe);
            plan_to_x(x, feedrate);
            if (y != coordinates.y_travel_safe) {
                plan_to_y(y);
            }
        };

        switch (point) {
        case VentCalibrationPoint::open_start:
            plan_to_point(coordinates.x_open_start, coordinates.y_open_start);
            break;
        case VentCalibrationPoint::open_end:
            plan_to_point(coordinates.x_open_start, coordinates.y_open_start);
            if (coordinates.y_open_start != coordinates.y_open_end) {
                plan_to_y(coordinates.y_open_end);
            }
            plan_to_x(coordinates.x_open_end, coordinates.feedrate);
            break;
        case VentCalibrationPoint::close_start:
            plan_to_point(coordinates.x_close_start, coordinates.y_close_start);
            break;
        case VentCalibrationPoint::close_end:
            plan_to_point(coordinates.x_close_start, coordinates.y_close_start);
            if (coordinates.y_close_start != coordinates.y_close_end) {
                plan_to_y(coordinates.y_close_end);
            }
            plan_to_x(coordinates.x_close_end, coordinates.feedrate);
            break;
        }
    }

}; // namespace

bool open() {
    PrintStatusMessageGuard psm_guard;
    psm_guard.update<PrintStatusMessage::Type::opening_chamber_vents>({});

    if (!before()) {
        return false;
    }
    switch_lever(VentState::open);
    buddy::chamber().set_vent_state(buddy::Chamber::VentState::open);
    after();
    planner.synchronize(); // Wait for all planned moves to complete
    return true;
}

bool close() {
    PrintStatusMessageGuard psm_guard;
    psm_guard.update<PrintStatusMessage::Type::closing_chamber_vents>({});

    if (!before()) {
        return false;
    }

    switch_lever(VentState::close);
    buddy::chamber().set_vent_state(buddy::Chamber::VentState::closed);
    after();
    planner.synchronize(); // Wait for all planned moves to complete
    return true;
}

bool move_to_calibration_point(uint8_t point) {
    if (point > VentCalibrationPoint::close_end) {
        return false;
    }

    if (!before()) {
        return false;
    }

    plan_to_calibration_point(selected_coordinates(), static_cast<VentCalibrationPoint>(point));
    planner.synchronize();
    return true;
}
} // namespace automatic_chamber_vents
