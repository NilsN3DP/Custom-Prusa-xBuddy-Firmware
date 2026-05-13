#include "menu_items_chamber.hpp"

#include <feature/chamber/chamber.hpp>
#include <img_resources.hpp>
#include <marlin/Configuration.h>
#include <numeric_input_config_common.hpp>
#include <cmath>
#include <cstdlib>
#include <timing.h>

using namespace buddy;

// MI_CHAMBER_TARGET_TEMP
// ============================================
MI_CHAMBER_TARGET_TEMP::MI_CHAMBER_TARGET_TEMP(const char *label)
    : WiSpin(0, numeric_input_config::chamber_temp_with_off(), _(label), &img::enclosure_16x16) //
{
    const auto caps = chamber().capabilities();
    set_is_hidden(!caps.always_show_temperature_control && !caps.temperature_control());
}

void MI_CHAMBER_TARGET_TEMP::OnClick() {
    chamber().set_target_temperature(value_opt());
}

void MI_CHAMBER_TARGET_TEMP::Loop() {
    if (is_edited()) {
        return;
    }

    const bool temp_ctrl = chamber().capabilities().temperature_control();
    const auto new_val = (temp_ctrl ? chamber().target_temperature() : std::nullopt);

    set_enabled(temp_ctrl);
    set_value(new_val);
}

// MI_CHAMBER_TEMP
// ============================================
MI_CHAMBER_TEMP::MI_CHAMBER_TEMP(const char *label)
    : WI_LAMBDA_LABEL_t(_(label), [this](const std::span<char> &buffer) {
        if (temperature_tenths_ == invalid_temperature) {
            strlcpy(buffer.data(), "N/A", buffer.size());
            return;
        }

        const int value = temperature_tenths_;
        snprintf(buffer.data(), buffer.size(), "%i.%u\xC2\xB0\x43", value / 10, static_cast<unsigned>(std::abs(value % 10)));
    }) //
{
    update_displayed_temperature();
    set_is_hidden(!chamber().capabilities().temperature_reporting);
}

bool MI_CHAMBER_TEMP::update_displayed_temperature() {
    const auto temperature = chamber().current_temperature();
    const int16_t new_value = temperature.has_value() ? static_cast<int16_t>(std::roundf(*temperature * 10.f)) : invalid_temperature;

    if (temperature_tenths_ == new_value) {
        return false;
    }

    temperature_tenths_ = new_value;
    return true;
}

void MI_CHAMBER_TEMP::Loop() {
    const auto now = ticks_ms();
    if (ticks_diff(now, last_update_ms_) < 1000) {
        return;
    }
    last_update_ms_ = now;

    if (update_displayed_temperature()) {
        InValidateExtension();
    }

    const bool should_hide = !chamber().capabilities().temperature_reporting;
    if (IsHidden() != should_hide) {
        set_is_hidden(should_hide);
    }
}
