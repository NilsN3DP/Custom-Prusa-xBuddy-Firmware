#pragma once

#include "ifooter_item.hpp"

class FooterItemConnectName final : public FooterIconText_IntVal {
    static int static_readValue();
    static string_view_utf8 static_makeView(int value);

public:
    FooterItemConnectName(window_t *parent);
};
