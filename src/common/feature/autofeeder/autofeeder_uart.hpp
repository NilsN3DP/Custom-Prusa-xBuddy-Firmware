/// @file
/// Binds the autofeeder driver to the printer's UART.

#pragma once

#include "autofeeder.hpp"

namespace buddy::autofeeder {

/// Opens the feeder UART and attaches it to \p instance().
///
/// Called once during startup. Nothing else has to happen for the feeder to be
/// found: the driver probes for a controller on its own and simply reports "not
/// connected" for as long as none answers, which is the normal state of a
/// printer without a feeder attached.
void init_uart_transport();

} // namespace buddy::autofeeder
