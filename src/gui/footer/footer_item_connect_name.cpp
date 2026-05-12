#include "footer_item_connect_name.hpp"

#include "img_resources.hpp"

#include <array>
#include <config_store/store_instance.hpp>
#include <cstring>

namespace {

constexpr const char fallback_name[] = "Prusa Connect";
constexpr size_t max_footer_name_len = 18;

uint32_t hostname_hash() {
    const char *hostname = config_store().hostname.get_c_str();
    uint32_t hash = 5381;

    while (hostname && *hostname) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(*hostname++);
    }

    return hash;
}

} // namespace

FooterItemConnectName::FooterItemConnectName(window_t *parent)
    : FooterIconText_IntVal(parent, &img::connect_16x16, static_makeView, static_readValue) {
}

int FooterItemConnectName::static_readValue() {
    return static_cast<int>(hostname_hash() & 0x7fffffff);
}

string_view_utf8 FooterItemConnectName::static_makeView([[maybe_unused]] int value) {
    static std::array<char, max_footer_name_len + 1> buffer {};
    const char *hostname = config_store().hostname.get_c_str();

    if (!hostname || hostname[0] == '\0') {
        hostname = fallback_name;
    }

    strlcpy(buffer.data(), hostname, buffer.size());
    if (strlen(hostname) > max_footer_name_len && buffer.size() >= 2) {
        buffer[buffer.size() - 2] = '.';
        buffer[buffer.size() - 1] = '\0';
    }

    return string_view_utf8::MakeRAM(buffer.data());
}
