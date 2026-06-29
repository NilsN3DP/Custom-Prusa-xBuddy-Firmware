#include "gui_theme.hpp"
#include <config_store/store_instance.hpp>
#include <algorithm>
#include <cstdint>

namespace gui::theme {

static Color accent_color_for(AccentColor a, uint16_t custom_hue, uint8_t custom_sat, uint8_t custom_val) {
    switch (a) {
    case AccentColor::filament_turquoise: return Color::from_raw(0x008F78);
    case AccentColor::bright_turquoise:  return Color::from_raw(0x00D4BB);
    case AccentColor::prusa_orange:      return Color::from_raw(0xF8651B);
    case AccentColor::blue:              return Color::from_raw(0x129DFF);
    case AccentColor::green:             return Color::from_raw(0x40B040);
    case AccentColor::purple:            return Color::from_raw(0x9B30FF);
    case AccentColor::pink:              return Color::from_raw(0xFF69B4);
    case AccentColor::red:               return Color::from_raw(0xE74626);
    case AccentColor::yellow:            return Color::from_raw(0xFFD700);
    case AccentColor::white:             return Color::from_raw(0xFFFFFF);
    case AccentColor::triforce_gold:     return Color::from_raw(0xD6A632);
    case AccentColor::custom: {
        const uint32_t h = custom_hue % 360;
        const uint32_t s = std::min<uint32_t>(custom_sat, 100);
        const uint32_t v = std::min<uint32_t>(custom_val, 100);
        const uint32_t v8 = v * 255 / 100;
        const uint32_t c8 = v8 * s / 100;
        const uint32_t m8 = v8 - c8;
        const uint32_t sector = h / 60;
        const uint32_t frac = h % 60;
        const uint32_t x8 = c8 * frac / 60;
        const uint32_t y8 = c8 * (60 - frac) / 60;
        uint8_t r, g, b;
        switch (sector) {
        case 0: r = c8; g = x8; b = 0; break;
        case 1: r = y8; g = c8; b = 0; break;
        case 2: r = 0; g = c8; b = x8; break;
        case 3: r = 0; g = y8; b = c8; break;
        case 4: r = x8; g = 0; b = c8; break;
        default: r = c8; g = 0; b = y8; break;
        }
        return Color::from_rgb(r + m8, g + m8, b + m8);
    }
    default: return Color::from_raw(0x008F78);
    }
}

static Color background_color_for(BackgroundColor b) {
    switch (b) {
    case BackgroundColor::white: return Color::from_raw(0xFFFFFF);
    case BackgroundColor::light_gray: return Color::from_raw(0xCCCCCC);
    case BackgroundColor::dark_gray: return Color::from_raw(0x222222);
    case BackgroundColor::black: return Color::from_raw(0x000000);
    case BackgroundColor::triforce_green: return Color::from_raw(0x00332E);
    default: return Color::from_raw(0xFFFFFF);
    }
}

static AccentColor preset_accent_for(ThemePreset p) {
    switch (p) {
    case ThemePreset::light_mint: return AccentColor::filament_turquoise;
    case ThemePreset::dark_mint: return AccentColor::bright_turquoise;
    case ThemePreset::prusa_classic: return AccentColor::prusa_orange;
    case ThemePreset::oled_dark: return AccentColor::white;
    case ThemePreset::graphite_cyan: return AccentColor::bright_turquoise;
    case ThemePreset::triforce_one: return AccentColor::triforce_gold;
    case ThemePreset::custom:
    default: return config_store().ui_accent_color.get();
    }
}

static BackgroundColor preset_background_for(ThemePreset p) {
    switch (p) {
    case ThemePreset::light_mint: return BackgroundColor::white;
    case ThemePreset::dark_mint: return BackgroundColor::dark_gray;
    case ThemePreset::prusa_classic: return BackgroundColor::black;
    case ThemePreset::oled_dark: return BackgroundColor::black;
    case ThemePreset::graphite_cyan: return BackgroundColor::dark_gray;
    case ThemePreset::triforce_one: return BackgroundColor::triforce_green;
    case ThemePreset::custom:
    default: return config_store().ui_background_color.get();
    }
}

static Color s_background { Color::from_raw(0xFFFFFF) };
static Color s_accent { Color::from_raw(0x008F78) };
static Color s_text { Color::from_raw(0x000000) };

static AccentColor s_accent_enum { AccentColor::filament_turquoise };
static BackgroundColor s_bg_enum { BackgroundColor::white };
static ThemePreset s_preset { ThemePreset::light_mint };
static uint32_t s_generation { 0 };

static void bump_generation() {
    ++s_generation;
    if (s_generation == 0) {
        ++s_generation;
    }
}

static void update_text_color() {
    const bool dark = (s_bg_enum == BackgroundColor::dark_gray || s_bg_enum == BackgroundColor::black || s_bg_enum == BackgroundColor::triforce_green);
    s_text = Color::from_raw(dark ? 0xFFFFFF : 0x000000);
}

ThemePreset preset() { return s_preset; }
AccentColor accent() { return s_accent_enum; }
BackgroundColor background() { return s_bg_enum; }

Color accent_color() { return s_accent; }
Color background_color() { return s_background; }
Color text_color() { return s_text; }
uint32_t generation() { return s_generation; }

static void set_accent_impl(AccentColor a) {
    const AccentColor old_enum = s_accent_enum;
    const Color old_accent = s_accent;
    s_accent_enum = a;
    s_accent = accent_color_for(a, config_store().ui_custom_hue.get(), config_store().ui_custom_saturation.get(), config_store().ui_custom_value.get());
    config_store().ui_accent_color.set(a);
    if (old_enum != s_accent_enum || old_accent != s_accent) {
        bump_generation();
    }
}

static void set_background_impl(BackgroundColor b) {
    const BackgroundColor old_enum = s_bg_enum;
    const Color old_background = s_background;
    const Color old_text = s_text;
    s_bg_enum = b;
    s_background = background_color_for(b);
    update_text_color();
    config_store().ui_background_color.set(b);
    if (old_enum != s_bg_enum || old_background != s_background || old_text != s_text) {
        bump_generation();
    }
}

void set_accent(AccentColor a) {
    s_preset = ThemePreset::custom;
    config_store().ui_theme_preset.set(s_preset);
    set_accent_impl(a);
}

void set_background(BackgroundColor b) {
    s_preset = ThemePreset::custom;
    config_store().ui_theme_preset.set(s_preset);
    set_background_impl(b);
}

void set_custom_hue(uint16_t hue) {
    config_store().ui_custom_hue.set(hue);
    s_preset = ThemePreset::custom;
    config_store().ui_theme_preset.set(s_preset);
    if (s_accent_enum == AccentColor::custom) {
        const Color old_accent = s_accent;
        s_accent = accent_color_for(AccentColor::custom, hue, config_store().ui_custom_saturation.get(), config_store().ui_custom_value.get());
        if (old_accent != s_accent) {
            bump_generation();
        }
    }
}

void set_custom_saturation(uint8_t saturation) {
    config_store().ui_custom_saturation.set(saturation);
    s_preset = ThemePreset::custom;
    config_store().ui_theme_preset.set(s_preset);
    if (s_accent_enum == AccentColor::custom) {
        const Color old_accent = s_accent;
        s_accent = accent_color_for(AccentColor::custom, config_store().ui_custom_hue.get(), saturation, config_store().ui_custom_value.get());
        if (old_accent != s_accent) {
            bump_generation();
        }
    }
}

void set_custom_value(uint8_t value) {
    config_store().ui_custom_value.set(value);
    s_preset = ThemePreset::custom;
    config_store().ui_theme_preset.set(s_preset);
    if (s_accent_enum == AccentColor::custom) {
        const Color old_accent = s_accent;
        s_accent = accent_color_for(AccentColor::custom, config_store().ui_custom_hue.get(), config_store().ui_custom_saturation.get(), value);
        if (old_accent != s_accent) {
            bump_generation();
        }
    }
}

void load_from_config_store() {
    s_preset = config_store().ui_theme_preset.get();
    set_accent_impl(preset_accent_for(s_preset));
    set_background_impl(preset_background_for(s_preset));
}

void apply_preset(ThemePreset p) {
    s_preset = p;
    config_store().ui_theme_preset.set(p);
    if (p == ThemePreset::custom) {
        set_accent_impl(config_store().ui_accent_color.get());
        set_background_impl(config_store().ui_background_color.get());
    } else {
        set_accent_impl(preset_accent_for(p));
        set_background_impl(preset_background_for(p));
    }
}

bool is_source_icon_accent(uint8_t r, uint8_t g, uint8_t b) {
    constexpr uint32_t max_distance_squared = 48u * 48u;
    const auto near = [&](uint8_t ref_r, uint8_t ref_g, uint8_t ref_b) {
        const int32_t dr = static_cast<int32_t>(r) - ref_r;
        const int32_t dg = static_cast<int32_t>(g) - ref_g;
        const int32_t db = static_cast<int32_t>(b) - ref_b;
        return static_cast<uint32_t>(dr * dr + dg * dg + db * db) <= max_distance_squared;
    };

    return near(248, 101, 27)
        || near(0, 143, 120)
        || near(0, 212, 187)
        || near(214, 166, 50);
}

Color remap_source_icon_accent(uint8_t r, uint8_t g, uint8_t b) {
    constexpr uint32_t ref_luma_orange = (77u * 248 + 150u * 101 + 29u * 27) >> 8;
    constexpr uint32_t ref_luma_turquoise = (77u * 0 + 150u * 143 + 29u * 120) >> 8;
    constexpr uint32_t ref_luma_gold = (77u * 214 + 150u * 166 + 29u * 50) >> 8;
    const uint32_t pixel_luma = (77u * r + 150u * g + 29u * b) >> 8;
    const uint32_t ref_luma = (r > g) ? ((g > 130) ? ref_luma_gold : ref_luma_orange) : ref_luma_turquoise;
    const Color acc = s_accent;
    auto scale = [&](uint8_t ch) -> uint8_t {
        const uint32_t result = ref_luma > 0 ? (static_cast<uint32_t>(ch) * pixel_luma + ref_luma / 2) / ref_luma : static_cast<uint32_t>(ch);
        return static_cast<uint8_t>(std::min<uint32_t>(result, 255u));
    };
    return Color::from_rgb(scale(acc.r), scale(acc.g), scale(acc.b));
}

} // namespace gui::theme
