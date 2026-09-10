#include "autofeeder_uart.hpp"

#include <device/peripherals_uart.hpp>

namespace buddy::autofeeder {

namespace {

    class UartTransport final : public Transport {
    public:
        size_t write(std::span<const uint8_t> data) override {
            return uart_for_autofeeder.Write(reinterpret_cast<const char *>(data.data()), data.size());
        }

        size_t read(std::span<uint8_t> data) override {
            return uart_for_autofeeder.Read(reinterpret_cast<char *>(data.data()), data.size());
        }
    };

    UartTransport transport;

} // namespace

void init_uart_transport() {
    uart_for_autofeeder.Open();

    // The driver polls; it must never block the Marlin server task waiting for
    // bytes that may not come at all.
    uart_for_autofeeder.SetReadTimeoutMs(0);

    instance().set_transport(&transport);
}

} // namespace buddy::autofeeder
