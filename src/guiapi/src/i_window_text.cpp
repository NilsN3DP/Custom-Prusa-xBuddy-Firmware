/**
 * @file i_window_text.cpp
 * @author Radek Vana
 * @date 2021-10-07
 */
#include "i_window_text.hpp"
#include "gui.hpp"
#include <gui_theme.hpp>

Color IWindowText::GetTextColor() const {
    if (flags.color_scheme_foreground && pTextColorScheme) {
        return pTextColorScheme->Get(IsFocused(), IsShadowed());
    }
    if (color_text == GuiDefaults::ColorText || color_text == GuiDefaults::COLOR_VALUE_VALID) {
        return gui::theme::text_color();
    }
    if (color_text == COLOR_GRAY || color_text == COLOR_SILVER || color_text == COLOR_LIGHT_GRAY || color_text == COLOR_DARK_GRAY
        || color_text == GuiDefaults::COLOR_VALUE_INVALID) {
        return gui::theme::secondary_text_color();
    }
    return color_text;
}

void IWindowText::SetTextColor(Color clr) {
    if (flags.color_scheme_foreground || color_text != clr) {
        color_text = clr;
        flags.color_scheme_foreground = false;
        Invalidate();
    }
}

void IWindowText::SetTextColor(const color_scheme &clr) {
    if ((!flags.color_scheme_foreground) || (!pTextColorScheme) || ((*pTextColorScheme) != clr)) {
        flags.color_scheme_foreground = true;
        Invalidate();
    }
    pTextColorScheme = &clr; // rewrite even when value is same, because address might be different
}

void IWindowText::set_font(Font val) {
    if (font != val) {
        font = val;
        Invalidate();
    }
}

Font IWindowText::get_font() const {
    return font;
}

void IWindowText::SetPadding(padding_ui8_t padd) {
    if (padding != padd) {
        padding = padd;
        Invalidate();
    }
}

IWindowText::IWindowText(window_t *parent, Rect16 rect, is_closed_on_click_t close)
    : window_aligned_t(parent, rect, win_type_t::normal, close)
    , color_text(GuiDefaults::ColorText)
    , font(GuiDefaults::DefaultFont)
    , padding(GuiDefaults::Padding) {
}
