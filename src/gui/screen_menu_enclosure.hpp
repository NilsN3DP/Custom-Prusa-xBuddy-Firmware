/**
 * @file screen_menu_enclosure.hpp
 * @brief displaying and setting of XL enclosure
 */

#pragma once

#include "screen_menu.hpp"
#include "MItem_enclosure.hpp"
#include "MItem_menus.hpp"
#include <MItem_tools.hpp>
#include "option/has_side_leds.h"
#include "option/has_toolchanger.h"
#include <option/has_chamber_filtration_api.h>
#include <device/board.h>
#include <gui/menu_item/specific/menu_items_chamber.hpp>
#include <option/has_leds_menu.h>
#include <option/has_xbuddy_extension.h>
#include <option/xbuddy_extension_variant.h>

#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
    #include <gui/menu_item/specific/menu_items_xbuddy_extension.hpp>
#endif

namespace detail {
using ScreenMenuEnclosure = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN
#if XL_ENCLOSURE_SUPPORT()
    ,
    MI_ENCLOSURE_ENABLE
#endif
#if HAS_CHAMBER_FILTRATION_API()
    ,
    MI_CHAMBER_FILTRATION
#endif
#if HAS_CHAMBER_API()
    ,
    MI_CHAMBER_TEMP
#endif
#if HAS_XBUDDY_EXTENSION() && XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
    ,
    MI_XBE_CHAMBER_LIGHTS,
    MI_XBUDDY_EXTENSION_COOLING_FANS,
    MI_XBUDDY_EXTENSION_COOLING_FANS_CONTROL_MAX,
    MI_XBE_FILTRATION_FAN
#endif
#if HAS_LEDS_MENU()
    ,
    MI_LEDS_SETTINGS
#endif
    >;
} // namespace detail

class ScreenMenuEnclosure : public detail::ScreenMenuEnclosure {

public:
    constexpr static const char *label = N_("ENCLOSURE SETTINGS");
    ScreenMenuEnclosure();
};
