#pragma once

#include <color.hpp>
#include <cstdint>

namespace gui::theme {

enum class AccentColor : uint8_t {
    filament_turquoise,
    bright_turquoise,
    prusa_orange,
    blue,
    green,
    purple,
    pink,
    red,
    yellow,
    white,
    custom,
    _count,
};

enum class BackgroundColor : uint8_t {
    white,
    light_gray,
    dark_gray,
    black,
    _count,
};

enum class ThemePreset : uint8_t {
    light_mint,
    dark_mint,
    prusa_classic,
    oled_dark,
    graphite_cyan,
    _count,
};

constexpr AccentColor default_accent = AccentColor::filament_turquoise;
constexpr BackgroundColor default_background = BackgroundColor::white;
constexpr ThemePreset default_preset = ThemePreset::light_mint;
constexpr Color source_icon_accent = Color::from_raw(0x008F78);

Color accent_color();
Color accent_color(AccentColor accent);
AccentColor accent();
void set_accent(AccentColor accent);
BackgroundColor background();
void set_background(BackgroundColor background);
ThemePreset preset();
void apply_preset(ThemePreset preset);
Color background_color();
Color text_color();
Color contrast_text_color(Color background);
Color selected_background_color();
Color selected_text_color();
Color menu_value_text_color(bool focused);
Color secondary_text_color();
Color disabled_text_color();
Color separator_color();
Color focus_indicator_color();
Color icon_neutral_color(uint8_t source_luma);
Color custom_accent_color();
void set_custom_hue(uint16_t hue);
void set_custom_saturation(uint8_t saturation);
void set_custom_value(uint8_t value);
bool is_source_icon_accent(uint8_t r, uint8_t g, uint8_t b);
Color remap_source_icon_accent(uint8_t r, uint8_t g, uint8_t b);

} // namespace gui::theme
