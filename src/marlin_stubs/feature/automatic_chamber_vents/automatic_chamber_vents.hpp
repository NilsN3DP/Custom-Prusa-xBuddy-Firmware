/// @file
#pragma once

#include <option/has_chamber_vents.h>

static_assert(HAS_CHAMBER_VENTS());

namespace automatic_chamber_vents {

/// @brief Opens the printer's vent grille.
/// @return true if the operation was successful, false otherwise.
bool open();

/// @brief Closes the printer's vent grille.
/// @return true if the operation was successful, false otherwise.
bool close();

/// @brief Moves to one selected vent calibration point without parking afterwards.
/// @param point Calibration point index: 0 open start, 1 open end, 2 close start, 3 close end.
/// @return true if the operation was successful, false otherwise.
bool move_to_calibration_point(uint8_t point);

} // namespace automatic_chamber_vents
