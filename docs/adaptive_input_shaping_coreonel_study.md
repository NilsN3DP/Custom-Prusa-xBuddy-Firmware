# Adaptive Input Shaping feasibility study for Core One L

## Scope

This branch starts from upstream `v6.5.6` and intentionally does not include the
N3DP UI/theme changes. The goal is a clean feasibility study for Core One L only:
use the permanently installed local accelerometer to decide during a print
whether the input-shaper parameters should be adjusted.

No UI changes are part of this first step.

## Reference target

Bambu Lab describes the A2L feature as adaptive vibration compensation with
multi-point calibration and load adaptation. The useful part to copy is the
principle, not the exact implementation: resonance compensation should account
for changing print conditions, especially tall or heavy models.

## Existing Prusa building blocks

- Core One L has `HAS_LOCAL_ACCELEROMETER()` enabled and routes the sensor
  through `PrusaAccelerometer`.
- Core One L already has CoreXY coordinate conversion in
  `PrusaAccelerometer::to_printer_coords()` and
  `PrusaAccelerometer::to_motor_coords()`.
- Current Core One L defaults are already separate from Core One:
  X = 56 Hz, Y = 42 Hz, both MZV.
- `M1959` performs input-shaper calibration by running controlled motion and
  analyzing accelerometer data.
- `M593` can change the active input-shaper config at runtime.

## Hard constraints found in the current code

1. `input_shaper::set_axis_config()` calls `planner.synchronize()`.
   This means changing shaper parameters is currently a blocking motion event,
   not something that can be done continuously inside the stepper loop.

2. `PrusaAccelerometer` is single-owner. The local implementation aborts with a
   BSOD if two owners try to access it at the same time. An adaptive feature must
   therefore own the accelerometer only while active and must not run during
   existing calibration flows.

3. Input shaping in this firmware is planned before step generation. Updating
   the shaper while buffered moves are already planned would require either
   draining/synchronizing the planner or deeper planner integration.

4. CoreXY axes are coupled. Updating only X or only Y needs the same
   pulse-alignment logic already used in `input_shaper_config.cpp`.

## Feasibility verdict

Feasible as an experimental Core One L feature, but not as true continuous
real-time adaptation in the first iteration.

The safe MVP is:

- monitor vibration during selected print windows,
- estimate whether the dominant X/Y resonance has drifted,
- apply small bounded frequency updates only at safe moments,
- never write adaptive values to EEPROM,
- fall back to the slicer/user `M593` values on print end or on confidence loss.

This should be treated as a guarded experiment, not a production replacement for
the existing input-shaper calibration.

## Proposed MVP without UI

Add a Core One L only experimental service:

```text
AdaptiveInputShaper
  states:
    disabled
    armed
    sampling
    pending_apply
    cooldown
    fault
```

Control it with a hidden/experimental G-code:

```gcode
M596 S0          ; disable adaptive shaping
M596 S1          ; enable adaptive shaping for this print/session
M596             ; print status and last estimate
M596 D1          ; debug log estimates to serial
```

Sampling policy:

- Start only when printing and Core One L is detected.
- Do not sample during homing, probing, calibration, crash recovery, pause,
  filament change, or power-panic recovery.
- Use short windows of accelerometer samples around naturally high-acceleration
  moves.
- Ignore windows with low acceleration, excessive Z motion, fan/nozzle events,
  or accelerometer overflow.

Estimator:

- Track X and Y separately in printer coordinates.
- Search only near current configured frequencies, for example +/- 8 Hz.
- Use a lightweight Goertzel or narrow-bin DFT estimator instead of a full FFT.
- Require multiple matching windows before accepting a frequency drift.
- Clamp all results with `input_shaper::clamp_frequency_to_safe_values()`.
- Limit updates to small deltas, for example 0.5-1.0 Hz per accepted update.

Apply policy:

- Apply with `input_shaper::set_axis_config()` only when the planner can be
  safely synchronized, initially at layer changes or other low-risk pauses.
- Do not apply more often than every N layers or every M seconds.
- Keep the filter type unchanged at first, likely MZV.
- Restore the original print-start config on print end or fault.

## Why not fully real-time yet?

Fully real-time adaptation would need a non-blocking way to swap shaper pulse
tables at a move boundary. The current public API intentionally synchronizes the
planner before changing the config. A deeper version could add a queued
"next-safe-motion-boundary" shaper update, but that touches motion planning and
step generation and should not be the first experiment.

## Files that matter

- `lib/Marlin/Marlin/src/feature/input_shaper/input_shaper_config.hpp`
- `lib/Marlin/Marlin/src/feature/input_shaper/input_shaper_config.cpp`
- `lib/Marlin/Marlin/src/gcode/feature/input_shaper/M593.cpp`
- `src/marlin_stubs/M1959.cpp`
- `lib/Marlin/Marlin/src/module/prusa/accelerometer.h`
- `lib/Marlin/Marlin/src/module/prusa/accelerometer_local.cpp`
- `src/buddy/main.cpp`

## First implementation steps

1. Add a disabled-by-default compile option for the experiment.
2. Add a Core One L only `AdaptiveInputShaper` module that can own the
   accelerometer during print.
3. Add `M596` for enable/disable/status/debug logging.
4. Implement sampling and serial-only reporting first, with no automatic
   applying.
5. Print test objects and compare estimated resonance drift against manual
   `M1959` before/after-print calibration.
6. Only after confidence is good, enable bounded automatic application at safe
   planner synchronization points.

## Open questions before writing active firmware code

- Does the accelerometer signal during normal extrusion have enough SNR for
  reliable resonance tracking without test moves?
- Which print events provide safe, repeatable sample windows on Core One L?
- How visible is a planner synchronization at layer changes?
- Does chamber temperature or bed mass produce measurable frequency drift on
  Core One L, or is static calibration already close enough?
- Should the first adaptive target be only Y/bed-load-like behavior, or both X
  and Y?
