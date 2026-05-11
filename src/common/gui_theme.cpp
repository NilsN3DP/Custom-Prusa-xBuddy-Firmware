#include "gui_theme.hpp"
#include <config_store/store_instance.hpp>
#include <algorithm>

namespace gui::theme {

static Color hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value) {
    hue %= 360;
    saturation = std::min<uint8_t>(saturation, 100);
    value = std::min<uint8_t>(value, 100);

    const uint16_t c = uint16_t(value) * saturation / 100;
    const uint16_t region = hue / 60;
    const uint16_t remainder = hue % 60;
    const uint16_t x = c * (60 - (region % 2 ? 60 - remainder : remainder)) / 60;
    const uint16_t m = value - c;

    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    switch (region) {
    case 0:
        r = c;
        g = x;
        break;
    case 1:
        r = x;
        g = c;
        break;
    case 2:
        g = c;
        b = x;
        break;
    case 3:
        g = x;
        b = c;
        break;
    case 4:
        r = x;
        b = c;
        break;
    default:
        r = c;
        b = x;
        break;
    }

    return Color::from_rgb(
        uint8_t((r + m) * 255 / 100),
        uint8_t((g + m) * 255 / 100),
        uint8_t((b + m) * 255 / 100));
}

Color accent_color(AccentColor accent) {
    switch (accent) {
    case AccentColor::bright_turquoise:
        return Color::from_raw(0x00D6B4);
    case AccentColor::prusa_orange:
        return COLOR_ORANGE;
    case AccentColor::blue:
        return Color::from_raw(0x129DFF);
    case AccentColor::green:
        return Color::from_raw(0x00A651);
    case AccentColor::purple:
        return Color::from_raw(0x7B2CFF);
    case AccentColor::pink:
        return Color::from_raw(0xE83E8C);
    case AccentColor::red:
        return Color::from_raw(0xE53935);
    case AccentColor::yellow:
        return Color::from_raw(0xF5B700);
    case AccentColor::white:
        return COLOR_WHITE;
    case AccentColor::custom:
        return custom_accent_color();
    case AccentColor::filament_turquoise:
    default:
        return source_icon_accent;
    }
}

AccentColor accent() {
    const auto value = config_store().ui_accent_color.get();
    if (std::to_underlying(value) >= std::to_underlying(AccentColor::_count)) {
        return default_accent;
    }
    return value;
}

Color accent_color() {
    return accent_color(accent());
}

void set_accent(AccentColor accent) {
    if (std::to_underlying(accent) >= std::to_underlying(AccentColor::_count)) {
        accent = default_accent;
    }
    config_store().ui_accent_color.set(accent);
}

BackgroundColor background() {
    const auto value = config_store().ui_background_color.get();
    if (std::to_underlying(value) >= std::to_underlying(BackgroundColor::_count)) {
        return default_background;
    }
    return value;
}

void set_background(BackgroundColor background) {
    if (std::to_underlying(background) >= std::to_underlying(BackgroundColor::_count)) {
        background = default_background;
    }
    config_store().ui_background_color.set(background);
}

ThemePreset preset() {
    const auto value = config_store().ui_theme_preset.get();
    if (std::to_underlying(value) >= std::to_underlying(ThemePreset::_count)) {
        return default_preset;
    }
    return value;
}

void apply_preset(ThemePreset preset) {
    if (std::to_underlying(preset) >= std::to_underlying(ThemePreset::_count)) {
        preset = default_preset;
    }

    config_store().ui_theme_preset.set(preset);

    switch (preset) {
    case ThemePreset::dark_mint:
        set_accent(AccentColor::filament_turquoise);
        set_background(BackgroundColor::black);
        break;
    case ThemePreset::prusa_classic:
        set_accent(AccentColor::prusa_orange);
        set_background(BackgroundColor::black);
        break;
    case ThemePreset::oled_dark:
        set_accent(AccentColor::bright_turquoise);
        set_background(BackgroundColor::black);
        break;
    case ThemePreset::graphite_cyan:
        set_accent(AccentColor::blue);
        set_background(BackgroundColor::dark_gray);
        break;
    case ThemePreset::light_mint:
    default:
        set_accent(AccentColor::filament_turquoise);
        set_background(BackgroundColor::white);
        break;
    }
}

Color background_color() {
    switch (background()) {
    case BackgroundColor::black:
        return COLOR_BLACK;
    case BackgroundColor::dark_gray:
        return COLOR_DARK_GRAY;
    case BackgroundColor::light_gray:
        return COLOR_LIGHT_GRAY;
    case BackgroundColor::white:
    default:
        return COLOR_WHITE;
    }
}

Color contrast_text_color(Color background) {
    const uint32_t luminance = uint32_t(background.r) * 299 + uint32_t(background.g) * 587 + uint32_t(background.b) * 114;
    return luminance > 128000 ? COLOR_BLACK : COLOR_WHITE;
}

Color text_color() {
    return contrast_text_color(background_color());
}

static uint8_t luma(Color color) {
    return uint8_t((uint32_t(color.r) * 299 + uint32_t(color.g) * 587 + uint32_t(color.b) * 114) / 1000);
}

static bool has_visible_contrast(Color a, Color b) {
    const uint8_t la = luma(a);
    const uint8_t lb = luma(b);
    return la > lb ? (la - lb) >= 70 : (lb - la) >= 70;
}

Color selected_background_color() {
    switch (background()) {
    case BackgroundColor::black:
        return COLOR_VERY_DARK_GRAY;
    case BackgroundColor::dark_gray:
        return COLOR_BLACK;
    case BackgroundColor::light_gray:
        return Color::from_raw(0x008A8A8A);
    case BackgroundColor::white:
    default:
        return Color::from_raw(0x00DDEEEB);
    }
}

Color selected_text_color() {
    return contrast_text_color(selected_background_color());
}

Color menu_value_text_color(bool focused) {
    return focused ? selected_text_color() : text_color();
}

Color secondary_text_color() {
    switch (background()) {
    case BackgroundColor::black:
        return COLOR_SILVER;
    case BackgroundColor::dark_gray:
        return COLOR_VERY_LIGHT_GRAY;
    case BackgroundColor::light_gray:
    case BackgroundColor::white:
    default:
        return COLOR_DARK_GRAY;
    }
}

Color disabled_text_color() {
    switch (background()) {
    case BackgroundColor::black:
    case BackgroundColor::dark_gray:
        return COLOR_LIGHT_GRAY;
    case BackgroundColor::light_gray:
    case BackgroundColor::white:
    default:
        return COLOR_DARK_GRAY;
    }
}

Color separator_color() {
    switch (background()) {
    case BackgroundColor::black:
        return COLOR_DARK_GRAY;
    case BackgroundColor::dark_gray:
        return COLOR_GRAY;
    case BackgroundColor::light_gray:
        return Color::from_raw(0x006D6D6D);
    case BackgroundColor::white:
    default:
        return COLOR_LIGHT_GRAY;
    }
}

Color focus_indicator_color() {
    const Color selected_background = selected_background_color();
    const Color accent = accent_color();
    if (has_visible_contrast(accent, selected_background)) {
        return accent;
    }
    return contrast_text_color(selected_background);
}

Color icon_neutral_color(uint8_t source_luma) {
    const Color foreground = text_color();
    const uint16_t scale = std::max<uint8_t>(source_luma, 40);

    return Color::from_rgb(
        uint8_t(uint16_t(foreground.r) * scale / 255),
        uint8_t(uint16_t(foreground.g) * scale / 255),
        uint8_t(uint16_t(foreground.b) * scale / 255));
}

Color custom_accent_color() {
    return hsv_to_rgb(config_store().ui_custom_hue.get(), config_store().ui_custom_saturation.get(), config_store().ui_custom_value.get());
}

void set_custom_hue(uint16_t hue) {
    config_store().ui_custom_hue.set(hue % 360);
}

void set_custom_saturation(uint8_t saturation) {
    config_store().ui_custom_saturation.set(std::min<uint8_t>(saturation, 100));
}

void set_custom_value(uint8_t value) {
    config_store().ui_custom_value.set(std::min<uint8_t>(value, 100));
}

static uint8_t max3(uint8_t a, uint8_t b, uint8_t c) {
    return std::max(std::max(a, b), c);
}

bool is_source_icon_accent(uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t max_channel = max3(r, g, b);
    if (max_channel < 18) {
        return false;
    }

    // Source accents are precolored to #008F78 and scaled darker for antialiasing.
    // Match that green-turquoise hue without touching grayscale icon strokes.
    return g > r + 16 && g >= b && b > r + 10;
}

Color remap_source_icon_accent(uint8_t r, uint8_t g, uint8_t b) {
    const Color target = accent_color();
    const uint8_t source_max = max3(source_icon_accent.r, source_icon_accent.g, source_icon_accent.b);
    const uint8_t pixel_max = max3(r, g, b);
    const uint16_t scale = source_max ? (uint16_t(pixel_max) * 255 / source_max) : 255;

    return Color::from_rgb(
        uint8_t(std::min<uint16_t>(uint16_t(target.r) * scale / 255, 255)),
        uint8_t(std::min<uint16_t>(uint16_t(target.g) * scale / 255, 255)),
        uint8_t(std::min<uint16_t>(uint16_t(target.b) * scale / 255, 255)));
}

} // namespace gui::theme
