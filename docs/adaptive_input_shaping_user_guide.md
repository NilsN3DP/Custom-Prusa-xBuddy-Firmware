# Adaptive Input Shaping Test Firmware

This custom firmware adds an experimental Adaptive Shaping switch to the Input Shaper menu.

## Enable

1. Run the normal Prusa Input Shaper calibration first.
2. Open `Settings > Input Shaper`.
3. Set `Adaptive Shaping` to `On`.
4. Start a print.

The switch is persistent, but the live frequency corrections are not written to EEPROM. At the end of the print, or when the switch is turned off, the firmware restores the input-shaper frequencies that were active when the print started.

## How It Works

During printing the firmware samples the local accelerometer continuously, looks for a stable resonance peak near the current X/Y Input Shaper frequencies, and nudges the active frequency by at most 0.35 Hz per update. The tracker uses short measurement windows and can update roughly every 1.5 seconds when the signal is stable enough.

This is intentionally conservative. It is meant for test prints and video experiments, not as a final replacement for Prusa's calibration workflow.

## Core One L

Core One L has the local accelerometer installed permanently, so it is the primary test target. Enable `Adaptive Shaping`, print a ringing or high-acceleration test, and compare the same G-code with the switch off and on.

## MK4 / MK4S

MK4/MK4S builds can use the same experimental code path, but the accelerometer must be mounted and connected correctly before printing. The firmware retries if the accelerometer is missing, but no adaptive correction is possible without usable samples.

## Test Ideas

- Ringing tower with straight X/Y walls.
- Same print at two accelerations, one conservative and one aggressive.
- Tall print where resonance changes with Z height.
- Compare normal Input Shaper only versus Adaptive Shaping enabled.

## Stop Conditions

Turn the switch off if you see pauses, artifacts, worse ringing, or accelerometer errors in logs. The feature is experimental and only changes the runtime input-shaper frequency while enabled.
