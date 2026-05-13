/**
 * @file footer_item_enclosure.hpp
 * @brief footer item displaying enclosure temperature
 */

#pragma once

#include "footer_items_heaters.hpp"

class FooterItemChamberTemperature final : public FooterItemHeater {
    static string_view_utf8 static_makeView(int value);
    static int static_readValue();

    virtual resized_t updateState() override;

public:
    FooterItemChamberTemperature(window_t *parent);
};
