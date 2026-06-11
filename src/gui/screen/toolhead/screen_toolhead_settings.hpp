#pragma once

#include <variant>
#include <cstdint>

#include "screen_toolhead_settings_common.hpp"

#include <config_store/store_definition.hpp>
#include <option/has_chamber_vents.h>
#include <option/has_mmu2.h>
#include <gui/menu_item/menu_item_select_menu.hpp>

#if HAS_MMU2()
    #include <MItem_mmu.hpp>
#endif

namespace screen_toolhead_settings {

class MI_NOZZLE_DIAMETER final : public MI_TOOLHEAD_SPECIFIC_SPIN {
public:
    MI_NOZZLE_DIAMETER(Toolhead toolhead = default_toolhead);
    float read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, float set) final;
};

class MI_NOZZLE_DIAMETER_HELP : public IWindowMenuItem {
public:
    MI_NOZZLE_DIAMETER_HELP();
    void click(IWindowMenu &menu) override;
};

#if HAS_HOTEND_TYPE_SUPPORT()
class MI_HOTEND_TYPE : public MI_TOOLHEAD_SPECIFIC<HotendType, MenuItemSelectMenu> {
public:
    MI_HOTEND_TYPE(Toolhead toolhead = default_toolhead);

    int item_count() const final;
    void build_item_text(int index, const std::span<char> &buffer) const final;

    void update();

    HotendType read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, HotendType set) final;

protected:
    bool on_item_selected(int old_index, int new_index) override;

private:
    bool has_varying_values_;
};

class MI_NOZZLE_SOCK : public MI_TOOLHEAD_SPECIFIC_TOGGLE {
public:
    MI_NOZZLE_SOCK(Toolhead toolhead = default_toolhead);

    bool read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, bool set) final;
};

using MI_HOTEND_SOCK_OR_TYPE = std::conditional_t<hotend_type_only_sock, MI_NOZZLE_SOCK, MI_HOTEND_TYPE>;
#endif

class MI_NOZZLE_HARDENED : public MI_TOOLHEAD_SPECIFIC_TOGGLE {
public:
    MI_NOZZLE_HARDENED(Toolhead toolhead = default_toolhead);
    bool read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, bool set) final;
};

class MI_NOZZLE_HIGH_FLOW : public MI_TOOLHEAD_SPECIFIC_TOGGLE {
public:
    MI_NOZZLE_HIGH_FLOW(Toolhead toolhead = default_toolhead);
    bool read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, bool set) final;
};

#if HAS_TOOLCHANGER()
class MI_DOCK : public MI_TOOLHEAD_SPECIFIC_BASE<IWindowMenuItem> {
public:
    MI_DOCK(Toolhead toolhead = default_toolhead);
    void click(IWindowMenu &) override;
    void update() final {}
};

class MI_PICK_PARK : public MI_TOOLHEAD_SPECIFIC_BASE<IWindowMenuItem> {
public:
    MI_PICK_PARK(Toolhead toolhead = default_toolhead);
    void update() final;
    void click(IWindowMenu &) override;

private:
    bool is_picked = false;
};
#endif

class MI_FILAMENT_SENSORS : public MI_TOOLHEAD_SPECIFIC_BASE<IWindowMenuItem> {
public:
    MI_FILAMENT_SENSORS(Toolhead toolhead = default_toolhead);
    void click(IWindowMenu &) override;
    void update() final {}
};

#if HAS_SELFTEST() && FILAMENT_SENSOR_IS_ADC()
class MI_CALIBRATE_FILAMENT_SENSORS : public MI_TOOLHEAD_SPECIFIC_BASE<IWindowMenuItem> {
public:
    MI_CALIBRATE_FILAMENT_SENSORS(Toolhead toolhead = default_toolhead);
    void update() final;
    void click(IWindowMenu &) override;
};
#endif

class MI_NOZZLE_OFFSET : public MI_TOOLHEAD_SPECIFIC_BASE<IWindowMenuItem> {
public:
    MI_NOZZLE_OFFSET(Toolhead toolhead = default_toolhead);
    void click(IWindowMenu &) override;
    void update() final {}
};

#if HAS_PRINT_FAN_TYPE()
class MI_PRINT_FAN_TYPE : public MI_TOOLHEAD_SPECIFIC<PrintFanType, MenuItemSelectMenu> {
public:
    MI_PRINT_FAN_TYPE(Toolhead toolhead = default_toolhead);

    int item_count() const final;
    void build_item_text(int index, const std::span<char> &buffer) const final;

    void update();

    PrintFanType read_value_impl(ToolheadIndex ix) final;
    void store_value_impl(ToolheadIndex ix, PrintFanType set) final;

protected:
    bool on_item_selected(int old_index, int new_index) override;

private:
    bool has_varying_values_;
};
#endif

#if HAS_CHAMBER_VENTS()
class MI_TOP_VENT_PARTS : public MenuItemSelectMenu {
public:
    MI_TOP_VENT_PARTS();

    int item_count() const final;
    void build_item_text(int index, const std::span<char> &buffer) const final;

protected:
    bool on_item_selected(int old_index, int new_index) override;
};

class MI_TOP_VENT_ACTION : public IWindowMenuItem {
public:
    enum class Action {
        open,
        close,
    };

    MI_TOP_VENT_ACTION(Action action);

protected:
    void click(IWindowMenu &) override;

private:
    Action action_;
};

using MI_TOP_VENT_OPEN = WithConstructorArgs<MI_TOP_VENT_ACTION, MI_TOP_VENT_ACTION::Action::open>;
using MI_TOP_VENT_CLOSE = WithConstructorArgs<MI_TOP_VENT_ACTION, MI_TOP_VENT_ACTION::Action::close>;

enum class TopVentN3dpCoord {
    open_start_x,
    open_end_x,
    close_start_x,
    close_end_x,
    safe_y,
    lever_y,
};

class MI_TOP_VENT_N3DP_POS : public WiSpin {
public:
    MI_TOP_VENT_N3DP_POS(TopVentN3dpCoord coord);

protected:
    void OnClick() override;

private:
    TopVentN3dpCoord coord_;
};

using MI_TOP_VENT_N3DP_OPEN_START_X = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::open_start_x>;
using MI_TOP_VENT_N3DP_OPEN_END_X = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::open_end_x>;
using MI_TOP_VENT_N3DP_CLOSE_START_X = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::close_start_x>;
using MI_TOP_VENT_N3DP_CLOSE_END_X = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::close_end_x>;
using MI_TOP_VENT_N3DP_SAFE_Y = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::safe_y>;
using MI_TOP_VENT_N3DP_LEVER_Y = WithConstructorArgs<MI_TOP_VENT_N3DP_POS, TopVentN3dpCoord::lever_y>;

class MI_TOP_VENT_CALIB_POINT : public MenuItemSelectMenu {
public:
    MI_TOP_VENT_CALIB_POINT();

    int item_count() const final;
    void build_item_text(int index, const std::span<char> &buffer) const final;

protected:
    bool on_item_selected(int old_index, int new_index) override;
};

class MI_TOP_VENT_CALIB_MOVE : public IWindowMenuItem {
public:
    MI_TOP_VENT_CALIB_MOVE();

protected:
    void click(IWindowMenu &) override;
};

class MI_TOP_VENT_CALIBRATE : public IWindowMenuItem {
public:
    MI_TOP_VENT_CALIBRATE();

protected:
    void click(IWindowMenu &) override;
};

class MI_TOP_VENT_CALIB_AXIS : public WiSpin {
public:
    enum class Axis {
        x,
        y,
    };

    MI_TOP_VENT_CALIB_AXIS(Axis axis);

protected:
    invalidate_t change(int dif) override;
    void OnClick() override;

public:
    void Loop() override;

private:
    Axis axis_;
    uint32_t generation_ = 0;
};

using MI_TOP_VENT_CALIB_X = WithConstructorArgs<MI_TOP_VENT_CALIB_AXIS, MI_TOP_VENT_CALIB_AXIS::Axis::x>;
using MI_TOP_VENT_CALIB_Y = WithConstructorArgs<MI_TOP_VENT_CALIB_AXIS, MI_TOP_VENT_CALIB_AXIS::Axis::y>;

class MI_TOP_VENT_CALIB_CONFIRM : public IWindowMenuItem {
public:
    MI_TOP_VENT_CALIB_CONFIRM();

protected:
    void click(IWindowMenu &) override;
};

using ScreenToolheadVentCalibration_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_TOP_VENT_CALIB_POINT,
    MI_TOP_VENT_CALIB_MOVE,
    MI_TOP_VENT_CALIB_X,
    MI_TOP_VENT_CALIB_Y,
    MI_TOP_VENT_CALIB_CONFIRM>;

class ScreenToolheadVentCalibration : public ScreenToolheadVentCalibration_ {
public:
    ScreenToolheadVentCalibration();
};

enum class TopVentCustomCoord {
    open_start_x,
    open_end_x,
    close_start_x,
    close_end_x,
    safe_y,
    lever_y,
};

class MI_TOP_VENT_CUSTOM_POS : public WiSpin {
public:
    MI_TOP_VENT_CUSTOM_POS(TopVentCustomCoord coord);

protected:
    void OnClick() override;

private:
    TopVentCustomCoord coord_;
};

using MI_TOP_VENT_CUSTOM_OPEN_START_X = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::open_start_x>;
using MI_TOP_VENT_CUSTOM_OPEN_END_X = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::open_end_x>;
using MI_TOP_VENT_CUSTOM_CLOSE_START_X = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::close_start_x>;
using MI_TOP_VENT_CUSTOM_CLOSE_END_X = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::close_end_x>;
using MI_TOP_VENT_CUSTOM_SAFE_Y = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::safe_y>;
using MI_TOP_VENT_CUSTOM_LEVER_Y = WithConstructorArgs<MI_TOP_VENT_CUSTOM_POS, TopVentCustomCoord::lever_y>;

using ScreenTopVentCustomSettings_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_TOP_VENT_N3DP_OPEN_START_X,
    MI_TOP_VENT_N3DP_OPEN_END_X,
    MI_TOP_VENT_N3DP_CLOSE_START_X,
    MI_TOP_VENT_N3DP_CLOSE_END_X,
    MI_TOP_VENT_N3DP_SAFE_Y,
    MI_TOP_VENT_N3DP_LEVER_Y,
    MI_TOP_VENT_CUSTOM_OPEN_START_X,
    MI_TOP_VENT_CUSTOM_OPEN_END_X,
    MI_TOP_VENT_CUSTOM_CLOSE_START_X,
    MI_TOP_VENT_CUSTOM_CLOSE_END_X,
    MI_TOP_VENT_CUSTOM_SAFE_Y,
    MI_TOP_VENT_CUSTOM_LEVER_Y>;

class ScreenTopVentCustomSettings : public ScreenTopVentCustomSettings_ {
public:
    ScreenTopVentCustomSettings();
};

class MI_TOP_VENT_CUSTOM_SETTINGS : public IWindowMenuItem {
public:
    MI_TOP_VENT_CUSTOM_SETTINGS();

protected:
    void click(IWindowMenu &) override;
};
#endif

#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
class MI_NOZZLE_CLEANING_PROFILE : public MenuItemSelectMenu {
public:
    MI_NOZZLE_CLEANING_PROFILE();

    int item_count() const final;
    void build_item_text(int index, const std::span<char> &buffer) const final;

protected:
    bool on_item_selected(int old_index, int new_index) override;
};

class MI_TEST_NOZZLE_CLEANING : public IWindowMenuItem {
public:
    MI_TEST_NOZZLE_CLEANING();

protected:
    void click(IWindowMenu &) override;
};

class MI_NOZZLE_CLEANING_TEMPERATURE : public WiSpin {
public:
    MI_NOZZLE_CLEANING_TEMPERATURE();

protected:
    void OnClick() override;
    void Loop() override;
};

enum class NozzleCleaningCustomCoord {
    start_x,
    start_y,
    end_x,
    end_y,
};

class MI_NOZZLE_CLEANING_CUSTOM_POS : public WiSpin {
public:
    MI_NOZZLE_CLEANING_CUSTOM_POS(NozzleCleaningCustomCoord coord);

protected:
    void OnClick() override;

private:
    NozzleCleaningCustomCoord coord_;
};

using MI_NOZZLE_CLEANING_CUSTOM_START_X = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_POS, NozzleCleaningCustomCoord::start_x>;
using MI_NOZZLE_CLEANING_CUSTOM_START_Y = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_POS, NozzleCleaningCustomCoord::start_y>;
using MI_NOZZLE_CLEANING_CUSTOM_END_X = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_POS, NozzleCleaningCustomCoord::end_x>;
using MI_NOZZLE_CLEANING_CUSTOM_END_Y = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_POS, NozzleCleaningCustomCoord::end_y>;

enum class NozzleCleaningCustomParam {
    passes,
    speed,
    fan,
};

class MI_NOZZLE_CLEANING_CUSTOM_PARAM : public WiSpin {
public:
    MI_NOZZLE_CLEANING_CUSTOM_PARAM(NozzleCleaningCustomParam param);

protected:
    void OnClick() override;

private:
    NozzleCleaningCustomParam param_;
};

using MI_NOZZLE_CLEANING_CUSTOM_PASSES = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_PARAM, NozzleCleaningCustomParam::passes>;
using MI_NOZZLE_CLEANING_CUSTOM_SPEED = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_PARAM, NozzleCleaningCustomParam::speed>;
using MI_NOZZLE_CLEANING_CUSTOM_FAN = WithConstructorArgs<MI_NOZZLE_CLEANING_CUSTOM_PARAM, NozzleCleaningCustomParam::fan>;

using ScreenNozzleCleaningCustomSettings_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_NOZZLE_CLEANING_CUSTOM_PASSES,
    MI_NOZZLE_CLEANING_CUSTOM_SPEED,
    MI_NOZZLE_CLEANING_CUSTOM_FAN,
    MI_NOZZLE_CLEANING_CUSTOM_START_X,
    MI_NOZZLE_CLEANING_CUSTOM_START_Y,
    MI_NOZZLE_CLEANING_CUSTOM_END_X,
    MI_NOZZLE_CLEANING_CUSTOM_END_Y>;

class ScreenNozzleCleaningCustomSettings : public ScreenNozzleCleaningCustomSettings_ {
public:
    ScreenNozzleCleaningCustomSettings();
};

class MI_NOZZLE_CLEANING_CUSTOM_SETTINGS : public IWindowMenuItem {
public:
    MI_NOZZLE_CLEANING_CUSTOM_SETTINGS();

protected:
    void click(IWindowMenu &) override;
};
#endif

using ScreenToolheadDetail_ = ScreenMenu<EFooter::Off,
    MI_RETURN,
    MI_NOZZLE_DIAMETER,
#if PRINTER_IS_PRUSA_XL()
    // Prusa XL was sold with .6mm nozzles and then with .4mm nozzles, so the users need to set in the FW what nozzles they have
    // This is to help them out a bit
    MI_NOZZLE_DIAMETER_HELP,
#endif
    MI_NOZZLE_HARDENED,
    MI_NOZZLE_HIGH_FLOW,
#if HAS_HOTEND_TYPE_SUPPORT()
    MI_HOTEND_SOCK_OR_TYPE,
#endif
#if HAS_MMU2()
    MI_MMU_NEXTRUDER_REWORK,
    MI_DONE_EXTRUDER_MAINTENANCE, // both for loadcell equipped printers and MK3.5
#endif
#if HAS_TOOLCHANGER()
    MI_PICK_PARK,
    MI_NOZZLE_OFFSET,
    MI_DOCK,
    #if HAS_PRINT_FAN_TYPE()
    MI_PRINT_FAN_TYPE,
    #endif

#endif
#if HAS_CHAMBER_VENTS()
    MI_TOP_VENT_PARTS,
    MI_TOP_VENT_CALIBRATE,
    MI_TOP_VENT_OPEN,
    MI_TOP_VENT_CLOSE,
#endif
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    MI_NOZZLE_CLEANING_PROFILE,
    MI_NOZZLE_CLEANING_TEMPERATURE,
    MI_TEST_NOZZLE_CLEANING,
#endif
#if HAS_SELFTEST() && FILAMENT_SENSOR_IS_ADC()
    MI_CALIBRATE_FILAMENT_SENSORS,
#endif
    MI_FILAMENT_SENSORS>;

class ScreenToolheadDetail : public ScreenToolheadDetail_ {
public:
    ScreenToolheadDetail(Toolhead toolhead = default_toolhead);

private:
    const Toolhead toolhead;
    StringViewUtf8Parameters<2> title_params;
};

class MI_TOOLHEAD : public IWindowMenuItem {
public:
    MI_TOOLHEAD(Toolhead toolhead);

protected:
    void click(IWindowMenu &) final;

private:
    const Toolhead toolhead;
    StringViewUtf8Parameters<2> label_params;
};

#if HAS_TOOLCHANGER()
template <typename>
struct ScreenToolheadSettingsList_;

template <size_t... i>
struct ScreenToolheadSettingsList_<std::index_sequence<i...>> {
    using T = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, WithConstructorArgs<MI_TOOLHEAD, ToolheadIndex(i)>..., WithConstructorArgs<MI_TOOLHEAD, AllToolheads {}>>;
};

class ScreenToolheadSettingsList : public ScreenToolheadSettingsList_<std::make_index_sequence<toolhead_count>>::T {
public:
    ScreenToolheadSettingsList();
};
#endif

} // namespace screen_toolhead_settings

using ScreenToolheadDetail = screen_toolhead_settings::ScreenToolheadDetail;

#if HAS_TOOLCHANGER()
using ScreenToolheadSettingsList = screen_toolhead_settings::ScreenToolheadSettingsList;
#endif
