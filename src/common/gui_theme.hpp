#pragma once

#include <cstdint>
#include <utils/color.hpp>

namespace gui::theme {

enum class ThemePreset : uint8_t {
    light_mint,
    dark_mint,
    prusa_classic,
    oled_dark,
    graphite_cyan,
    triforce_one,
    custom,
    _count,
};

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
    triforce_gold,
    custom,
    _count,
};

enum class BackgroundColor : uint8_t {
    white,
    light_gray,
    dark_gray,
    black,
    triforce_green,
    _count,
};

inline constexpr ThemePreset default_preset = ThemePreset::light_mint;
inline constexpr AccentColor default_accent = AccentColor::filament_turquoise;
inline constexpr BackgroundColor default_background = BackgroundColor::white;

ThemePreset preset();
AccentColor accent();
BackgroundColor background();

Color accent_color();
Color background_color();
Color text_color();
uint32_t generation();

void load_from_config_store();
void apply_preset(ThemePreset p);
void set_accent(AccentColor a);
void set_background(BackgroundColor b);
void set_custom_hue(uint16_t hue);
void set_custom_saturation(uint8_t saturation);
void set_custom_value(uint8_t value);

bool is_source_icon_accent(uint8_t r, uint8_t g, uint8_t b);
Color remap_source_icon_accent(uint8_t r, uint8_t g, uint8_t b);

} // namespace gui::theme
