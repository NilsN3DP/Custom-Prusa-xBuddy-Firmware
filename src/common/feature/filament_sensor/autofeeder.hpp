/**
 * @file
 * CORE One auxiliary filament autofeeder control.
 */
#pragma once

namespace buddy::autofeeder {

void cycle();
bool start_manual();
bool start_calibration(float expected_distance_mm, float max_deviation_mm, float max_deviation_pct);
void stop();
bool is_running();
void reset_error();
void report_status();

} // namespace buddy::autofeeder
