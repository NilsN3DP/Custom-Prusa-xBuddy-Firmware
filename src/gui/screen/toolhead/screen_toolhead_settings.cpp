#include "screen_toolhead_settings.hpp"

#include <common/nozzle_diameter.hpp>
#include <marlin_client.hpp>
#include <ScreenFactory.hpp>
#include <ScreenHandler.hpp>
#include <img_resources.hpp>
#include <gui/dialogs/window_dlg_wait.hpp>
#include <numeric_input_config_common.hpp>
#include <gcode/queue.h>
#include <module/planner.h>
#include <utils/string_builder.hpp>
#include <algorithm>

#include "screen_toolhead_settings_fs.hpp"
#include "screen_toolhead_settings_dock.hpp"
#include "screen_toolhead_settings_nozzle_offset.hpp"

using namespace screen_toolhead_settings;

static constexpr NumericInputConfig nozzle_diameter_spin_config_with_special = [] {
    NumericInputConfig result = nozzle_diameter_spin_config;
    result.special_value = 0;
    result.special_value_str = N_("-");
    return result;
}();

#if HAS_CHAMBER_VENTS()
static constexpr NumericInputConfig top_vent_position_spin_config = {
    .min_value = -50.f,
    .max_value = 80.f,
    .unit = Unit::millimeter,
};

static constexpr const char *top_vent_parts_items[] = {
    N_("Original Parts"),
    N_("Modded N3DP Parts"),
    N_("Custom Vent Position"),
};

static constexpr const char *top_vent_calibration_point_items[] = {
    N_("Open Start"),
    N_("Open End"),
    N_("Close Start"),
    N_("Close End"),
};

struct TopVentCalibrationPosition {
    float x;
    float y;
};

struct TopVentCalibrationSession {
    uint8_t point = 0;
    TopVentCalibrationPosition position = {};
    uint32_t generation = 1;
};

static TopVentCalibrationSession top_vent_calibration_session;

enum TopVentPartsProfile : uint8_t {
    original_parts = 0,
    modded_n3dp_parts = 1,
    custom_vent_position = 2,
};

enum TopVentCalibrationPoint : uint8_t {
    open_start = 0,
    open_end = 1,
    close_start = 2,
    close_end = 3,
};

static TopVentCalibrationPosition top_vent_original_position(uint8_t point) {
    switch (point) {
    case open_start:
        return { 37.f, -3.f };
    case open_end:
        return { 24.f, -18.f };
    case close_start:
        return { 11.f, -3.f };
    case close_end:
        return { 26.f, -18.f };
    }
    return {};
}

static TopVentCalibrationPosition top_vent_n3dp_position(uint8_t point) {
    switch (point) {
    case open_start:
        return { config_store().top_vent_n3dp_open_start_x.get(), config_store().top_vent_n3dp_open_start_y.get() };
    case open_end:
        return { config_store().top_vent_n3dp_open_end_x.get(), config_store().top_vent_n3dp_open_end_y.get() };
    case close_start:
        return { config_store().top_vent_n3dp_close_start_x.get(), config_store().top_vent_n3dp_close_start_y.get() };
    case close_end:
        return { config_store().top_vent_n3dp_close_end_x.get(), config_store().top_vent_n3dp_close_end_y.get() };
    }
    return {};
}

static TopVentCalibrationPosition top_vent_custom_position(uint8_t point) {
    switch (point) {
    case open_start:
        return { config_store().top_vent_custom_open_start_x.get(), config_store().top_vent_custom_open_start_y.get() };
    case open_end:
        return { config_store().top_vent_custom_open_end_x.get(), config_store().top_vent_custom_open_end_y.get() };
    case close_start:
        return { config_store().top_vent_custom_close_start_x.get(), config_store().top_vent_custom_close_start_y.get() };
    case close_end:
        return { config_store().top_vent_custom_close_end_x.get(), config_store().top_vent_custom_close_end_y.get() };
    }
    return {};
}

static TopVentCalibrationPosition top_vent_selected_position(uint8_t point) {
    switch (config_store().top_vent_parts_profile.get()) {
    case modded_n3dp_parts:
        return top_vent_n3dp_position(point);
    case custom_vent_position:
        return top_vent_custom_position(point);
    case original_parts:
    default:
        return top_vent_original_position(point);
    }
}

static float top_vent_selected_safe_y() {
    switch (config_store().top_vent_parts_profile.get()) {
    case modded_n3dp_parts:
        return config_store().top_vent_n3dp_safe_y.get();
    case custom_vent_position:
        return config_store().top_vent_custom_safe_y.get();
    case original_parts:
    default:
        return -3.f;
    }
}

static void top_vent_save_n3dp_position(uint8_t point, TopVentCalibrationPosition position) {
    switch (point) {
    case open_start:
        config_store().top_vent_n3dp_open_start_x.set(position.x);
        config_store().top_vent_n3dp_open_start_y.set(position.y);
        break;
    case open_end:
        config_store().top_vent_n3dp_open_end_x.set(position.x);
        config_store().top_vent_n3dp_open_end_y.set(position.y);
        break;
    case close_start:
        config_store().top_vent_n3dp_close_start_x.set(position.x);
        config_store().top_vent_n3dp_close_start_y.set(position.y);
        break;
    case close_end:
        config_store().top_vent_n3dp_close_end_x.set(position.x);
        config_store().top_vent_n3dp_close_end_y.set(position.y);
        break;
    }
}

static void top_vent_save_custom_position(uint8_t point, TopVentCalibrationPosition position) {
    switch (point) {
    case open_start:
        config_store().top_vent_custom_open_start_x.set(position.x);
        config_store().top_vent_custom_open_start_y.set(position.y);
        break;
    case open_end:
        config_store().top_vent_custom_open_end_x.set(position.x);
        config_store().top_vent_custom_open_end_y.set(position.y);
        break;
    case close_start:
        config_store().top_vent_custom_close_start_x.set(position.x);
        config_store().top_vent_custom_close_start_y.set(position.y);
        break;
    case close_end:
        config_store().top_vent_custom_close_end_x.set(position.x);
        config_store().top_vent_custom_close_end_y.set(position.y);
        break;
    }
}

static void top_vent_load_calibration_session(uint8_t point) {
    top_vent_calibration_session.point = std::min<uint8_t>(point, close_end);
    top_vent_calibration_session.position = top_vent_selected_position(top_vent_calibration_session.point);
    ++top_vent_calibration_session.generation;
}

static void top_vent_move_to_calibration_session_position() {
    const double safe_y = top_vent_selected_safe_y();
    marlin_client::gcode_printf(
        "G0 Y%.2f F3000\n"
        "G0 X%.2f F3000\n"
        "G0 Y%.2f F3000",
        safe_y,
        static_cast<double>(top_vent_calibration_session.position.x),
        static_cast<double>(top_vent_calibration_session.position.y));
}

static void top_vent_save_calibration_session() {
    const auto profile = config_store().top_vent_parts_profile.get();
    if (profile == modded_n3dp_parts) {
        top_vent_save_n3dp_position(top_vent_calibration_session.point, top_vent_calibration_session.position);
    } else {
        if (profile == original_parts) {
            config_store().top_vent_parts_profile.set(custom_vent_position);
        }
        top_vent_save_custom_position(top_vent_calibration_session.point, top_vent_calibration_session.position);
    }
}

static const char *top_vent_n3dp_coord_label(TopVentN3dpCoord coord) {
    switch (coord) {
    case TopVentN3dpCoord::open_start_x:
        return N_("N3DP Open Start X");
    case TopVentN3dpCoord::open_end_x:
        return N_("N3DP Open End X");
    case TopVentN3dpCoord::close_start_x:
        return N_("N3DP Close Start X");
    case TopVentN3dpCoord::close_end_x:
        return N_("N3DP Close End X");
    case TopVentN3dpCoord::safe_y:
        return N_("N3DP Safe Y");
    case TopVentN3dpCoord::lever_y:
        return N_("N3DP Lever Y");
    }
    return N_("N3DP Vent Pos");
}

static float top_vent_n3dp_coord_get(TopVentN3dpCoord coord) {
    switch (coord) {
    case TopVentN3dpCoord::open_start_x:
        return config_store().top_vent_n3dp_open_start_x.get();
    case TopVentN3dpCoord::open_end_x:
        return config_store().top_vent_n3dp_open_end_x.get();
    case TopVentN3dpCoord::close_start_x:
        return config_store().top_vent_n3dp_close_start_x.get();
    case TopVentN3dpCoord::close_end_x:
        return config_store().top_vent_n3dp_close_end_x.get();
    case TopVentN3dpCoord::safe_y:
        return config_store().top_vent_n3dp_open_start_y.get();
    case TopVentN3dpCoord::lever_y:
        return config_store().top_vent_n3dp_open_end_y.get();
    }
    return 0.f;
}

static void top_vent_n3dp_coord_set(TopVentN3dpCoord coord, float value) {
    switch (coord) {
    case TopVentN3dpCoord::open_start_x:
        config_store().top_vent_n3dp_open_start_x.set(value);
        break;
    case TopVentN3dpCoord::open_end_x:
        config_store().top_vent_n3dp_open_end_x.set(value);
        break;
    case TopVentN3dpCoord::close_start_x:
        config_store().top_vent_n3dp_close_start_x.set(value);
        break;
    case TopVentN3dpCoord::close_end_x:
        config_store().top_vent_n3dp_close_end_x.set(value);
        break;
    case TopVentN3dpCoord::safe_y:
        config_store().top_vent_n3dp_safe_y.set(value);
        config_store().top_vent_n3dp_open_start_y.set(value);
        config_store().top_vent_n3dp_close_start_y.set(value);
        break;
    case TopVentN3dpCoord::lever_y:
        config_store().top_vent_n3dp_lever_y.set(value);
        config_store().top_vent_n3dp_open_end_y.set(value);
        config_store().top_vent_n3dp_close_end_y.set(value);
        break;
    }
}

static const char *top_vent_custom_coord_label(TopVentCustomCoord coord) {
    switch (coord) {
    case TopVentCustomCoord::open_start_x:
        return N_("Vent Open Start X");
    case TopVentCustomCoord::open_end_x:
        return N_("Vent Open End X");
    case TopVentCustomCoord::close_start_x:
        return N_("Vent Close Start X");
    case TopVentCustomCoord::close_end_x:
        return N_("Vent Close End X");
    case TopVentCustomCoord::safe_y:
        return N_("Vent Safe Y");
    case TopVentCustomCoord::lever_y:
        return N_("Vent Lever Y");
    }
    return N_("Vent Position");
}

static float top_vent_custom_coord_get(TopVentCustomCoord coord) {
    switch (coord) {
    case TopVentCustomCoord::open_start_x:
        return config_store().top_vent_custom_open_start_x.get();
    case TopVentCustomCoord::open_end_x:
        return config_store().top_vent_custom_open_end_x.get();
    case TopVentCustomCoord::close_start_x:
        return config_store().top_vent_custom_close_start_x.get();
    case TopVentCustomCoord::close_end_x:
        return config_store().top_vent_custom_close_end_x.get();
    case TopVentCustomCoord::safe_y:
        return config_store().top_vent_custom_open_start_y.get();
    case TopVentCustomCoord::lever_y:
        return config_store().top_vent_custom_open_end_y.get();
    }
    return 0.f;
}

static void top_vent_custom_coord_set(TopVentCustomCoord coord, float value) {
    switch (coord) {
    case TopVentCustomCoord::open_start_x:
        config_store().top_vent_custom_open_start_x.set(value);
        break;
    case TopVentCustomCoord::open_end_x:
        config_store().top_vent_custom_open_end_x.set(value);
        break;
    case TopVentCustomCoord::close_start_x:
        config_store().top_vent_custom_close_start_x.set(value);
        break;
    case TopVentCustomCoord::close_end_x:
        config_store().top_vent_custom_close_end_x.set(value);
        break;
    case TopVentCustomCoord::safe_y:
        config_store().top_vent_custom_safe_y.set(value);
        config_store().top_vent_custom_open_start_y.set(value);
        config_store().top_vent_custom_close_start_y.set(value);
        break;
    case TopVentCustomCoord::lever_y:
        config_store().top_vent_custom_lever_y.set(value);
        config_store().top_vent_custom_open_end_y.set(value);
        config_store().top_vent_custom_close_end_y.set(value);
        break;
    }
}
#endif

#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
static constexpr NumericInputConfig nozzle_cleaning_position_spin_config = {
    .min_value = 0.f,
    .max_value = 330.f,
    .unit = Unit::millimeter,
};

static constexpr NumericInputConfig nozzle_cleaning_passes_spin_config = {
    .min_value = 1,
    .max_value = 10,
};

static constexpr NumericInputConfig nozzle_cleaning_speed_spin_config = {
    .min_value = 500,
    .max_value = 8000,
    .step = 100,
};

static constexpr const char *nozzle_cleaning_profile_items[] = {
    N_("Standard"),
    N_("Printed Wiper"),
    N_("Custom"),
};

static uint16_t nozzle_cleaning_profile_temperature_get() {
    switch (config_store().nozzle_cleaning_profile.get()) {
    case 1:
        return config_store().nozzle_cleaning_printed_wiper_temperature.get();
    case 2:
        return config_store().nozzle_cleaning_custom_temperature.get();
    case 0:
    default:
        return config_store().nozzle_cleaning_standard_temperature.get();
    }
}

static void nozzle_cleaning_profile_temperature_set(uint16_t temperature) {
    switch (config_store().nozzle_cleaning_profile.get()) {
    case 1:
        config_store().nozzle_cleaning_printed_wiper_temperature.set(temperature);
        break;
    case 2:
        config_store().nozzle_cleaning_custom_temperature.set(temperature);
        break;
    case 0:
    default:
        config_store().nozzle_cleaning_standard_temperature.set(temperature);
        break;
    }
}

static const char *nozzle_cleaning_custom_coord_label(NozzleCleaningCustomCoord coord) {
    switch (coord) {
    case NozzleCleaningCustomCoord::start_x:
        return N_("Nozzle Clean Start X");
    case NozzleCleaningCustomCoord::start_y:
        return N_("Nozzle Clean Start Y");
    case NozzleCleaningCustomCoord::end_x:
        return N_("Nozzle Clean End X");
    case NozzleCleaningCustomCoord::end_y:
        return N_("Nozzle Clean End Y");
    }
    return N_("Nozzle Clean Pos");
}

static float nozzle_cleaning_custom_coord_get(NozzleCleaningCustomCoord coord) {
    switch (coord) {
    case NozzleCleaningCustomCoord::start_x:
        return config_store().nozzle_cleaning_custom_start_x.get();
    case NozzleCleaningCustomCoord::start_y:
        return config_store().nozzle_cleaning_custom_start_y.get();
    case NozzleCleaningCustomCoord::end_x:
        return config_store().nozzle_cleaning_custom_end_x.get();
    case NozzleCleaningCustomCoord::end_y:
        return config_store().nozzle_cleaning_custom_end_y.get();
    }
    return 0.f;
}

static void nozzle_cleaning_custom_coord_set(NozzleCleaningCustomCoord coord, float value) {
    switch (coord) {
    case NozzleCleaningCustomCoord::start_x:
        config_store().nozzle_cleaning_custom_start_x.set(value);
        break;
    case NozzleCleaningCustomCoord::start_y:
        config_store().nozzle_cleaning_custom_start_y.set(value);
        break;
    case NozzleCleaningCustomCoord::end_x:
        config_store().nozzle_cleaning_custom_end_x.set(value);
        break;
    case NozzleCleaningCustomCoord::end_y:
        config_store().nozzle_cleaning_custom_end_y.set(value);
        break;
    }
}

static const char *nozzle_cleaning_custom_param_label(NozzleCleaningCustomParam param) {
    switch (param) {
    case NozzleCleaningCustomParam::passes:
        return N_("Brush Passes");
    case NozzleCleaningCustomParam::speed:
        return N_("Brush Speed");
    case NozzleCleaningCustomParam::fan:
        return N_("Brush Fan");
    }
    return "";
}

static float nozzle_cleaning_custom_param_get(NozzleCleaningCustomParam param) {
    switch (param) {
    case NozzleCleaningCustomParam::passes:
        return config_store().nozzle_cleaning_custom_passes.get();
    case NozzleCleaningCustomParam::speed:
        return config_store().nozzle_cleaning_custom_speed.get();
    case NozzleCleaningCustomParam::fan:
        return config_store().nozzle_cleaning_custom_fan.get();
    }
    return 0;
}

static void nozzle_cleaning_custom_param_set(NozzleCleaningCustomParam param, float value) {
    switch (param) {
    case NozzleCleaningCustomParam::passes:
        config_store().nozzle_cleaning_custom_passes.set(static_cast<uint8_t>(value));
        break;
    case NozzleCleaningCustomParam::speed:
        config_store().nozzle_cleaning_custom_speed.set(static_cast<uint16_t>(value));
        break;
    case NozzleCleaningCustomParam::fan:
        config_store().nozzle_cleaning_custom_fan.set(static_cast<uint8_t>(value));
        break;
    }
}

static const NumericInputConfig &nozzle_cleaning_custom_param_config(NozzleCleaningCustomParam param) {
    switch (param) {
    case NozzleCleaningCustomParam::passes:
        return nozzle_cleaning_passes_spin_config;
    case NozzleCleaningCustomParam::speed:
        return nozzle_cleaning_speed_spin_config;
    case NozzleCleaningCustomParam::fan:
        return numeric_input_config::percent_with_off;
    }
    return nozzle_cleaning_passes_spin_config;
}
#endif

// * MI_NOZZLE_DIAMETER
MI_NOZZLE_DIAMETER::MI_NOZZLE_DIAMETER(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_SPIN(toolhead, 0, nozzle_diameter_spin_config_with_special, _("Nozzle Diameter")) {
    update();
}

float MI_NOZZLE_DIAMETER::read_value_impl(ToolheadIndex ix) {
    return config_store().get_nozzle_diameter(ix);
}

void MI_NOZZLE_DIAMETER::store_value_impl(ToolheadIndex ix, float set) {
    config_store().set_nozzle_diameter(ix, set);
}

// * MI_NOZZLE_DIAMETER_HELP
MI_NOZZLE_DIAMETER_HELP::MI_NOZZLE_DIAMETER_HELP()
    : IWindowMenuItem(_("What nozzle diameter do I have?"), &img::question_16x16) {
}

void MI_NOZZLE_DIAMETER_HELP::click(IWindowMenu &) {
    MsgBoxInfo(_("You can determine the nozzle diameter by counting the markings (dots) on the nozzle:\n"
                 "  0.40 mm nozzle: 3 dots\n"
                 "  0.60 mm nozzle: 4 dots\n\n"
                 "For more information, visit prusa.io/nozzle-types"),
        Responses_Ok);
}

#if HAS_HOTEND_TYPE_SUPPORT()
// * MI_HOTEND_TYPE
MI_HOTEND_TYPE::MI_HOTEND_TYPE(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC(toolhead, _("Hotend Type")) {
    update();
}

int MI_HOTEND_TYPE::item_count() const {
    // If has varying values, the 0th item is "-" (for different values)
    return hotend_type_list.size() + (has_varying_values_ ? 1 : 0);
}

void MI_HOTEND_TYPE::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    const int effective_index = index - (has_varying_values_ ? 1 : 0);

    // If has varying values, the 0th item is "-" (for different values)
    if (effective_index == -1) {
        sb.append_string("-");
    } else {
        sb.append_string_view(_(hotend_type_name(hotend_type_list[effective_index])));
    }
}

bool MI_HOTEND_TYPE::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    const int effective_index = new_index - (has_varying_values_ ? 1 : 0);

    if (effective_index == -1) {
        return false;
    }

    if (!msgbox_confirm_change(toolhead(), user_already_confirmed_changes_)) {
        return false;
    }

    store_value(hotend_type_list[effective_index]);
    return true;
}

void MI_HOTEND_TYPE::update() {
    const auto val = read_value();
    has_varying_values_ = !val.has_value();

    // If has varying values, the 0th item is "-" (for different values)
    // Force set - we might be changing item texts here
    force_set_current_item(has_varying_values_ ? 0 : stdext::index_of(hotend_type_list, *val));
}

HotendType MI_HOTEND_TYPE::read_value_impl(ToolheadIndex ix) {
    return config_store().hotend_type.get(ix);
}

void MI_HOTEND_TYPE::store_value_impl(ToolheadIndex ix, HotendType set) {
    config_store().hotend_type.set(ix, set);
}

// * MI_NOZZLE_SOCK
MI_NOZZLE_SOCK::MI_NOZZLE_SOCK(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_TOGGLE(toolhead, false, _("Nextruder Silicone Sock")) {
    update();
}

bool MI_NOZZLE_SOCK::read_value_impl(ToolheadIndex ix) {
    return config_store().hotend_type.get(ix) == HotendType::stock_with_sock;
}

void MI_NOZZLE_SOCK::store_value_impl(ToolheadIndex ix, bool set) {
    config_store().hotend_type.set(ix, set ? HotendType::stock_with_sock : HotendType::stock);
}
#endif /* HAS_HOTEND_TYPE_SUPPORT() */

#if HAS_PRINT_FAN_TYPE()
MI_PRINT_FAN_TYPE::MI_PRINT_FAN_TYPE(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC(toolhead, _("Print Fan Type")) {
    update();
}

PrintFanType MI_PRINT_FAN_TYPE::read_value_impl(ToolheadIndex ix) {
    return get_print_fan_type(ix);
}

void MI_PRINT_FAN_TYPE::store_value_impl(ToolheadIndex ix, PrintFanType set) {
    set_print_fan_type(ix, set);
}

void MI_PRINT_FAN_TYPE::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    const int effective_index = index - (has_varying_values_ ? 1 : 0);

    // If has varying values, the 0th item is "-" (for different values)
    if (effective_index == -1) {
        sb.append_string("-");
    } else {
        sb.append_string_view(_(print_fan_type_names[print_fan_type_list[effective_index]]));
    }
}

bool MI_PRINT_FAN_TYPE::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    const int effective_index = new_index - (has_varying_values_ ? 1 : 0);

    if (effective_index == -1) {
        return false;
    }

    if (!msgbox_confirm_change(toolhead(), user_already_confirmed_changes_)) {
        return false;
    }

    store_value(print_fan_type_list[effective_index]);
    return true;
}

void MI_PRINT_FAN_TYPE::update() {
    const auto val = read_value();
    has_varying_values_ = !val.has_value();

    // If has varying values, the 0th item is "-" (for different values)
    // Force set - we might be changing item texts here
    force_set_current_item(has_varying_values_ ? 0 : stdext::index_of(print_fan_type_list, *val));
}

int MI_PRINT_FAN_TYPE::item_count() const {
    // If has varying values, the 0th item is "-" (for different values)
    return print_fan_type_list.size() + (has_varying_values_ ? 1 : 0);
}
#endif /* HAS_PRINT_FAN_TYPE */

#if HAS_CHAMBER_VENTS()
// * MI_TOP_VENT_PARTS
MI_TOP_VENT_PARTS::MI_TOP_VENT_PARTS()
    : MenuItemSelectMenu(_("Top Vent Parts")) {
    set_current_item(config_store().top_vent_parts_profile.get());
}

int MI_TOP_VENT_PARTS::item_count() const {
    return sizeof(top_vent_parts_items) / sizeof(top_vent_parts_items[0]);
}

void MI_TOP_VENT_PARTS::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    sb.append_string_view(_(top_vent_parts_items[index]));
}

bool MI_TOP_VENT_PARTS::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    config_store().top_vent_parts_profile.set(new_index);
    return true;
}

// * MI_TOP_VENT_ACTION
MI_TOP_VENT_ACTION::MI_TOP_VENT_ACTION(Action action)
    : IWindowMenuItem(action == Action::open ? _("Vent Open") : _("Vent Close"))
    , action_(action) {
}

void MI_TOP_VENT_ACTION::click(IWindowMenu &) {
    marlin_client::gcode(action_ == Action::open ? "M870 O" : "M870 C");
}

// * MI_TOP_VENT_CALIB_POINT
MI_TOP_VENT_CALIB_POINT::MI_TOP_VENT_CALIB_POINT()
    : MenuItemSelectMenu(_("Vent Point")) {
    set_current_item(config_store().top_vent_calibration_point.get());
}

int MI_TOP_VENT_CALIB_POINT::item_count() const {
    return sizeof(top_vent_calibration_point_items) / sizeof(top_vent_calibration_point_items[0]);
}

void MI_TOP_VENT_CALIB_POINT::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    sb.append_string_view(_(top_vent_calibration_point_items[index]));
}

bool MI_TOP_VENT_CALIB_POINT::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    config_store().top_vent_calibration_point.set(new_index);
    top_vent_load_calibration_session(new_index);
    return true;
}

// * MI_TOP_VENT_CALIB_MOVE
MI_TOP_VENT_CALIB_MOVE::MI_TOP_VENT_CALIB_MOVE()
    : IWindowMenuItem(_("Move to Vent Point")) {
}

void MI_TOP_VENT_CALIB_MOVE::click(IWindowMenu &) {
    top_vent_load_calibration_session(config_store().top_vent_calibration_point.get());
    marlin_client::gcode_printf("M870 P%i", config_store().top_vent_calibration_point.get());
}

// * MI_TOP_VENT_CALIBRATE
MI_TOP_VENT_CALIBRATE::MI_TOP_VENT_CALIBRATE()
    : IWindowMenuItem(_("Vent Opener Tuning"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_TOP_VENT_CALIBRATE::click(IWindowMenu &) {
    top_vent_load_calibration_session(config_store().top_vent_calibration_point.get());
    Screens::Access()->Open(ScreenFactory::Screen<ScreenToolheadVentCalibration>);
}

// * MI_TOP_VENT_CALIB_AXIS
MI_TOP_VENT_CALIB_AXIS::MI_TOP_VENT_CALIB_AXIS(Axis axis)
    : WiSpin(axis == Axis::x ? top_vent_calibration_session.position.x : top_vent_calibration_session.position.y, top_vent_position_spin_config, axis == Axis::x ? _("Vent X Position") : _("Vent Y Position"))
    , axis_(axis)
    , generation_(top_vent_calibration_session.generation) {
}

invalidate_t MI_TOP_VENT_CALIB_AXIS::change(int dif) {
    const auto result = WiSpin::change(dif);
    if (result == invalidate_t::yes) {
        if (axis_ == Axis::x) {
            top_vent_calibration_session.position.x = value();
        } else {
            top_vent_calibration_session.position.y = value();
        }
        top_vent_move_to_calibration_session_position();
    }
    return result;
}

void MI_TOP_VENT_CALIB_AXIS::OnClick() {
    if (axis_ == Axis::x) {
        top_vent_calibration_session.position.x = value();
    } else {
        top_vent_calibration_session.position.y = value();
    }
    top_vent_move_to_calibration_session_position();
}

void MI_TOP_VENT_CALIB_AXIS::Loop() {
    if (generation_ != top_vent_calibration_session.generation) {
        generation_ = top_vent_calibration_session.generation;
        set_value(axis_ == Axis::x ? top_vent_calibration_session.position.x : top_vent_calibration_session.position.y);
    }
}

// * MI_TOP_VENT_CALIB_CONFIRM
MI_TOP_VENT_CALIB_CONFIRM::MI_TOP_VENT_CALIB_CONFIRM()
    : IWindowMenuItem(_("Save Vent Position")) {
}

void MI_TOP_VENT_CALIB_CONFIRM::click(IWindowMenu &) {
    top_vent_save_calibration_session();
    config_store().top_vent_calibration_point.set(top_vent_calibration_session.point);
    Screens::Access()->Close();
}

// * ScreenToolheadVentCalibration
ScreenToolheadVentCalibration::ScreenToolheadVentCalibration()
    : ScreenMenu(_("VENT OPENER")) {
    top_vent_load_calibration_session(config_store().top_vent_calibration_point.get());
    marlin_client::gcode_printf("M870 P%i", config_store().top_vent_calibration_point.get());
}

// * MI_TOP_VENT_N3DP_POS
MI_TOP_VENT_N3DP_POS::MI_TOP_VENT_N3DP_POS(TopVentN3dpCoord coord)
    : WiSpin(top_vent_n3dp_coord_get(coord), top_vent_position_spin_config, _(top_vent_n3dp_coord_label(coord)))
    , coord_(coord) {
}

void MI_TOP_VENT_N3DP_POS::OnClick() {
    top_vent_n3dp_coord_set(coord_, value());
}

// * MI_TOP_VENT_CUSTOM_POS
MI_TOP_VENT_CUSTOM_POS::MI_TOP_VENT_CUSTOM_POS(TopVentCustomCoord coord)
    : WiSpin(top_vent_custom_coord_get(coord), top_vent_position_spin_config, _(top_vent_custom_coord_label(coord)))
    , coord_(coord) {
}

void MI_TOP_VENT_CUSTOM_POS::OnClick() {
    top_vent_custom_coord_set(coord_, value());
}

// * ScreenTopVentCustomSettings
ScreenTopVentCustomSettings::ScreenTopVentCustomSettings()
    : ScreenMenu(_("CUSTOM VENT")) {
}

// * MI_TOP_VENT_CUSTOM_SETTINGS
MI_TOP_VENT_CUSTOM_SETTINGS::MI_TOP_VENT_CUSTOM_SETTINGS()
    : IWindowMenuItem(_("Custom Vent"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_TOP_VENT_CUSTOM_SETTINGS::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenTopVentCustomSettings>);
}
#endif

#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
// * MI_NOZZLE_CLEANING_PROFILE
MI_NOZZLE_CLEANING_PROFILE::MI_NOZZLE_CLEANING_PROFILE()
    : MenuItemSelectMenu(_("Nozzle Cleaning")) {
    set_current_item(config_store().nozzle_cleaning_profile.get());
}

int MI_NOZZLE_CLEANING_PROFILE::item_count() const {
    return sizeof(nozzle_cleaning_profile_items) / sizeof(nozzle_cleaning_profile_items[0]);
}

void MI_NOZZLE_CLEANING_PROFILE::build_item_text(int index, const std::span<char> &buffer) const {
    StringBuilder sb(buffer);
    sb.append_string_view(_(nozzle_cleaning_profile_items[index]));
}

bool MI_NOZZLE_CLEANING_PROFILE::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    config_store().nozzle_cleaning_profile.set(new_index);
    return true;
}

// * MI_TEST_NOZZLE_CLEANING
MI_TEST_NOZZLE_CLEANING::MI_TEST_NOZZLE_CLEANING()
    : IWindowMenuItem(_("Nozzle Clean Test")) {
}

void MI_TEST_NOZZLE_CLEANING::click(IWindowMenu &) {
    marlin_client::gcode("G12 R");
}

// * MI_NOZZLE_CLEANING_TEMPERATURE
MI_NOZZLE_CLEANING_TEMPERATURE::MI_NOZZLE_CLEANING_TEMPERATURE()
    : WiSpin(nozzle_cleaning_profile_temperature_get(), numeric_input_config::nozzle_temperature, _("Cleaning Temp")) {
}

void MI_NOZZLE_CLEANING_TEMPERATURE::OnClick() {
    nozzle_cleaning_profile_temperature_set(static_cast<uint16_t>(value()));
}

void MI_NOZZLE_CLEANING_TEMPERATURE::Loop() {
    set_value(nozzle_cleaning_profile_temperature_get());
}

// * MI_NOZZLE_CLEANING_CUSTOM_POS
MI_NOZZLE_CLEANING_CUSTOM_POS::MI_NOZZLE_CLEANING_CUSTOM_POS(NozzleCleaningCustomCoord coord)
    : WiSpin(nozzle_cleaning_custom_coord_get(coord), nozzle_cleaning_position_spin_config, _(nozzle_cleaning_custom_coord_label(coord)))
    , coord_(coord) {
}

void MI_NOZZLE_CLEANING_CUSTOM_POS::OnClick() {
    nozzle_cleaning_custom_coord_set(coord_, value());
}

// * MI_NOZZLE_CLEANING_CUSTOM_PARAM
MI_NOZZLE_CLEANING_CUSTOM_PARAM::MI_NOZZLE_CLEANING_CUSTOM_PARAM(NozzleCleaningCustomParam param)
    : WiSpin(nozzle_cleaning_custom_param_get(param), nozzle_cleaning_custom_param_config(param), _(nozzle_cleaning_custom_param_label(param)))
    , param_(param) {
}

void MI_NOZZLE_CLEANING_CUSTOM_PARAM::OnClick() {
    nozzle_cleaning_custom_param_set(param_, value());
}

// * ScreenNozzleCleaningCustomSettings
ScreenNozzleCleaningCustomSettings::ScreenNozzleCleaningCustomSettings()
    : ScreenMenu(_("CUSTOM WIPER")) {
}

// * MI_NOZZLE_CLEANING_CUSTOM_SETTINGS
MI_NOZZLE_CLEANING_CUSTOM_SETTINGS::MI_NOZZLE_CLEANING_CUSTOM_SETTINGS()
    : IWindowMenuItem(_("Custom Wiper"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_NOZZLE_CLEANING_CUSTOM_SETTINGS::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenNozzleCleaningCustomSettings>);
}
#endif

// * MI_NOZZLE_HARDENED
MI_NOZZLE_HARDENED::MI_NOZZLE_HARDENED(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_TOGGLE(toolhead, false, _("Nozzle Hardened")) //
{
    update();
}

bool MI_NOZZLE_HARDENED::read_value_impl(ToolheadIndex ix) {
    return config_store().nozzle_is_hardened.get().test(ix);
}

void MI_NOZZLE_HARDENED::store_value_impl(ToolheadIndex ix, bool set) {
    config_store().nozzle_is_hardened.apply([&](auto &item) {
        item.set(ix, set);
    });
}

// * MI_NOZZLE_HIGH_FLOW
MI_NOZZLE_HIGH_FLOW::MI_NOZZLE_HIGH_FLOW(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_TOGGLE(toolhead, false, _("Nozzle High-flow")) //
{
    update();
}

bool MI_NOZZLE_HIGH_FLOW::read_value_impl(ToolheadIndex ix) {
    return config_store().nozzle_is_high_flow.get().test(ix);
}

void MI_NOZZLE_HIGH_FLOW::store_value_impl(ToolheadIndex ix, bool set) {
    config_store().nozzle_is_high_flow.apply([&](auto &item) {
        item.set(ix, set);
    });
}

#if HAS_TOOLCHANGER()
// * MI_DOCK
MI_DOCK::MI_DOCK(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, _("Dock Position"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_DOCK::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolheadDetailDock>(toolhead()));
}

// * MI_PICK_PARK
MI_PICK_PARK::MI_PICK_PARK(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, string_view_utf8()) {
    update();
}

void MI_PICK_PARK::update() {
    const auto picked_tool = prusa_toolchanger.detect_tool_nr();
    is_picked = (toolhead() == all_toolheads) ? (picked_tool != PrusaToolChanger::MARLIN_NO_TOOL_PICKED) : (picked_tool == std::get<ToolheadIndex>(toolhead()));

    // Do not show "Pick Tool" at all for all_toolheads, only "Park Tool" that gets disabled when no tool is selected
    SetLabel(_(is_picked || (toolhead() == all_toolheads) ? N_("Park Tool") : N_("Pick Tool")));

    // If we're in all toolheads mode, allow only unpicking the tool
    set_enabled((toolhead() != all_toolheads) || is_picked);
}

void MI_PICK_PARK::click(IWindowMenu &) {
    marlin_client::gcode("G27 P0 Z5"); // Lift Z if not high enough
    marlin_client::gcode_printf("T%d S1 L0 D0", (!is_picked && toolhead() != all_toolheads) ? std::get<ToolheadIndex>(toolhead()) : PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    window_dlg_wait_t::wait_for_gcodes_to_finish();
    update();
}

#endif

// * MI_FILAMENT_SENSORS
MI_FILAMENT_SENSORS::MI_FILAMENT_SENSORS(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, _("Filament Sensors Tuning"), nullptr, is_enabled_t::yes, is_hidden_t::dev, expands_t::yes) {}

void MI_FILAMENT_SENSORS::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolheadDetailFS>(toolhead()));
}

#if HAS_SELFTEST() && FILAMENT_SENSOR_IS_ADC()
// * MI_CALIBRATE_FILAMENT_SENSORS
MI_CALIBRATE_FILAMENT_SENSORS::MI_CALIBRATE_FILAMENT_SENSORS(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, string_view_utf8()) {
    update();
}

void MI_CALIBRATE_FILAMENT_SENSORS::update() {
    SetLabel((HAS_SIDE_FSENSOR() || toolhead() == all_toolheads) ? _("Calibrate Filament Sensors") : _("Calibrate Filament Sensor"));
}

void MI_CALIBRATE_FILAMENT_SENSORS::click(IWindowMenu &) {
    if (MsgBoxQuestion(_("Perform filament sensors calibration? This discards previous filament sensors calibration."), Responses_YesNo) == Response::No) {
        return;
    }

    if (toolhead() == all_toolheads) {
        marlin_client::gcode_printf("M1981 F%i", (1 << HOTENDS) - 1);
    } else {
        marlin_client::gcode_printf("M1981 T%i", std::get<ToolheadIndex>(toolhead()));
    }
}
#endif

// * MI_NOZZLE_OFFSET
MI_NOZZLE_OFFSET::MI_NOZZLE_OFFSET(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, _("Nozzle Offset"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_NOZZLE_OFFSET::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolheadDetailNozzleOffset>(toolhead()));
}

// * ScreenToolheadDetail
ScreenToolheadDetail::ScreenToolheadDetail(Toolhead toolhead)
    : ScreenMenu({})
    , toolhead(toolhead) //
{

#if HAS_TOOLCHANGER()
    if (toolhead == all_toolheads) {
        header.SetText(_("ALL TOOLS"));
    } else if (prusa_toolchanger.is_toolchanger_enabled()) {
        header.SetText(_("TOOL %d").formatted(title_params, std::get<ToolheadIndex>(toolhead) + 1));
    } else {
        header.SetText(_("TOOLHEAD"));
    }
#else
    header.SetText(_("PRINTHEAD"));
#endif

    menu_set_toolhead(container, toolhead);

    // Do not show certain items until printer setup is done
    if (!config_store().printer_hw_config_done.get()) {
#if HAS_TOOLCHANGER()
        container.Item<MI_DOCK>().set_is_hidden();
        container.Item<MI_NOZZLE_OFFSET>().set_is_hidden();
        container.Item<MI_PICK_PARK>().set_is_hidden();
#endif
#if HAS_SELFTEST() && FILAMENT_SENSOR_IS_ADC()
        container.Item<MI_CALIBRATE_FILAMENT_SENSORS>().set_is_hidden();
#endif
    }

    // Some options don't make sense for AllToolheads
    if (toolhead == all_toolheads) {
#if HAS_TOOLCHANGER()
        container.Item<MI_DOCK>().set_is_hidden();
        container.Item<MI_NOZZLE_OFFSET>().set_is_hidden();
#endif
    }

    // Some options don't make sense for the default toolhead
    if (toolhead == default_toolhead) {
#if HAS_TOOLCHANGER()
        // Nozzle offset is always relative to the first tool, so it does not make sense to calibrate it for tool 0
        container.Item<MI_NOZZLE_OFFSET>().set_is_hidden();
#endif
    }

#if HAS_TOOLCHANGER()
    // Some options don't make sense if a toolchanger is disabled (single-tool XL)
    if (!prusa_toolchanger.is_toolchanger_enabled()) {
        container.Item<MI_PICK_PARK>().set_is_hidden();
        container.Item<MI_DOCK>().set_is_hidden();
    }
#endif
}

// * MI_TOOLHEAD
MI_TOOLHEAD::MI_TOOLHEAD(Toolhead toolhead)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , toolhead(toolhead) //
{
    if (toolhead == all_toolheads) {
        SetLabel(_("All Tools"));

    } else {
        const ToolheadIndex ix = std::get<ToolheadIndex>(toolhead);
        SetLabel(_("Tool %d").formatted(label_params, ix + 1));
#if HAS_TOOLCHANGER()
        set_is_hidden(!prusa_toolchanger.is_tool_enabled(ix));
#endif
    }
}

void MI_TOOLHEAD::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolheadDetail>(toolhead));
}

#if HAS_TOOLCHANGER()

// * ScreenToolheadSettingsList
ScreenToolheadSettingsList::ScreenToolheadSettingsList()
    : ScreenMenu(_("TOOLS SETTINGS")) //
{}

#endif
