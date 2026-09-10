# Automatic filament feeder (autofeeder)

An autofeeder is an external, motorised filament feeder that pushes filament from
the spool through the long PTFE tube up to the printer's **side filament
sensor**, and pulls it back out again on unload. On the XL this replaces the
manual "push the filament in until the printer notices it" step: the user only
has to poke the filament into the feeder's inlet.

The implementation is modelled on the Snapmaker U1's filament feeder
(`klippy/extras/filament_feed.py` in <https://github.com/Snapmaker/u1-klipper>,
GPL-3.0), whose mechanics — a PWM DC motor, an ADC inlet sensor, pinch-wheel
tachometers and a status LED per channel — this driver expects on the other end
of the wire. The drive levels, the ramp-up behaviour and the error set are taken
from there because they are proven against real hardware; see the constants in
`src/common/feature/autofeeder/autofeeder_load.hpp`.

## Enabling it

The feature is off by default on every printer. Build with:

```bash
python utils/build.py --preset xl --build-type release -- -DHAS_AUTOFEEDER=YES
```

`HAS_AUTOFEEDER` requires `HAS_SIDE_FSENSOR`, because the side sensor is what
tells the printer that the filament has arrived. Enabling it on a printer without
one fails at configure time.

A firmware built with the feature enabled still works with no feeder attached:
the driver probes for a controller, finds none, reports "not connected" and every
load/unload behaves exactly as before.

## How the work is split

The feeder controller is deliberately dumb. It knows how to run one motor
forward or backward at a given drive level for at most a given length or a given
time, and it reports what its sensors see. Everything else is decided by the
printer:

| Decision | Where it is made | Why |
| --- | --- | --- |
| Filament sits in the inlet | Controller | Its own ADC port sensor |
| Motor stalled / filament slipping | Controller | Its own motor and wheel tachometers |
| Filament arrived at the printer | **Printer** | The side filament sensor is read by xlBuddy's ADC mux, not by the feeder |
| When to load, retract or give up | **Printer** | It owns the load/unload sequence |

Because the arrival check lives in the printer, every movement the printer starts
carries a length limit *and* a timeout. The controller enforces both by itself,
so a printer that stops talking — a crash, a reset, an unplugged cable — cannot
leave a motor running.

## Wiring

The printer talks to the controller over a plain 3.3 V UART at 115200 baud, 8N1,
no flow control. On xlBuddy the driver uses **USART6 — PC6 (TX), PC7 (RX)**, with
DMA2 stream 2 (RX) and stream 7 (TX). None of these is claimed by anything else
on that board, and the GPIO/DMA setup already exists in
`src/device/stm32f4/hal_msp.cpp`.

> **Check this against your board before soldering.** xlBuddy has no dedicated
> connector for these pins; they have to be picked up from a test point or a
> spare header on the revision you own. To move the port, change
> `UART_AUTOFEEDER` in `src/device/stm32f4/peripherals_uart.cpp` and the matching
> ISRs in `src/device/stm32f4/interrupts_XLBUDDY.cpp`.

Ground must be common between the printer and the feeder controller. The feeder
motors need their own supply; do not draw them from the printer.

## Wire protocol

The protocol is defined in `src/common/feature/autofeeder/autofeeder_protocol.hpp`
together with `autofeeder_types.hpp`. Both headers plus
`autofeeder_protocol.cpp` are free of firmware dependencies and are meant to be
copied verbatim into the controller's own firmware, so that both ends cannot
disagree about the framing.

```
+------+------+------+------+--------+-----------------+---------+
| 0xA5 | 0x5A | cmd  | seq  | length | payload[length] | crc16   |
+------+------+------+------+--------+-----------------+---------+
```

* CRC-16/CCITT-FALSE over the command byte through the last payload byte.
* All multi-byte fields are little endian.
* The controller only ever speaks when spoken to. A response repeats the request's
  `cmd` with bit 7 set and the request's `seq`.
* The printer retries a request three times, 100 ms apart, before it declares the
  controller lost.

| Command | Payload | Response |
| --- | --- | --- |
| `0x01` info | – | `InfoResponse` (protocol version, channel count, fw version) |
| `0x02` get_status | channel | `StatusResponse` |
| `0x03` feed | `FeedRequest` (channel, direction, duty, max length, timeout) | `StatusResponse` |
| `0x04` stop | channel | `StatusResponse` |
| `0x05` set_led | channel, red, white | – |
| `0x7F` error | – | `ProtocolError` |

A `feed` on a channel that is already moving replaces the running movement,
including its length budget — the printer subtracts the distance already moved
when it re-issues a feed to raise the drive level.

### What the controller has to implement

* Report `protocol_version` = 1 in `info`, otherwise the printer refuses to talk
  to it.
* Stop the motor when the requested length or timeout is reached, and set the
  channel to `stopped`.
* Stop the motor and set the channel to `failed` with `no_filament` when the
  inlet port goes empty during a movement.
* Set `failed` with `motor_speed` when the motor tacho stays below its threshold
  while driven, and with `wheel_speed` when the wheels do not follow the motor
  (Snapmaker's thresholds: motor below 200 rpm, or wheel rpm × 33 below motor rpm
  × 0.3 — the gearbox ratio is 33:1 and the slip allowance 0.7).
* Measure moved length from the pinch wheel tachometers, 6 pulses per revolution
  over a 31.4159 mm wheel circumference on the Snapmaker mechanics.

## Sequences

### Load

`Pause` reaches `LoadState::await_filament` when the side sensor is empty. From
there, per `step()` of the Marlin server:

1. If no feeder channel exists for the tool, nothing changes — the user pushes the
   filament in by hand as before.
2. Otherwise the printer waits for the feeder to report filament at the inlet.
3. It starts a forward feed at 45 % duty to draw the filament into the drive
   wheels, then 70 %, ramping by 10 % every 350 ms up to 100 % while the filament
   has not arrived.
4. As soon as the side sensor reports filament, the printer stops the channel.
   The existing `assist_insertion` → `load_to_gears` → purge sequence takes over
   unchanged.
5. If the filament leaves the inlet again, the printer goes back to waiting — the
   user can simply push it in once more.
6. Any other failure raises `WarningType::FilamentLoadingTimeout` and stops the
   load.

### Unload

After a plain unload (not a filament change), `unload_finish_or_change_process`
enters `LoadState::autofeeder_retract`: the feeder pulls backwards until the side
sensor goes clear and then a further 1200 mm to bring the tip back to the inlet.
A failure here only warns — the filament is already out of the extruder, so the
unload itself succeeded.

Filament changes deliberately skip the retract: the filament has to stay in the
tube for the load that immediately follows.

## Diagnostics

`M1720` reports the controller and every channel:

```
> M1720
autofeeder: connected fw 1.0 channels 2
  ch0: at_inlet err=ok inlet=1 motor_rpm=0 wheel_rpm=0 moved_mm=0.00
  ch1: idle err=ok inlet=0 motor_rpm=0 wheel_rpm=0 moved_mm=0.00
```

For bring-up it can also drive a channel by hand:

```
M1720 P0 L200        ; push 200 mm on channel 0
M1720 P0 L-200 S255  ; pull 200 mm back at full drive level
M1720 P0 X           ; stop channel 0
```

These movements are still bounded by the controller's own length and timeout
watchdogs.

## Tests

`tests/unit/common/feature/autofeeder/` covers the framing, the driver's
request/response handling (retries, CRC recovery, losing the controller) and the
load/retract sequences against a fake controller that speaks the real protocol.

```bash
cmake -S . -B build_tests -G Ninja -DBOARD=BUDDY -DUNITTESTS_ENABLE=YES -DPNG2FONT_ENABLE=NO
ninja -C build_tests autofeeder_tests
./build_tests/tests/unit/common/feature/autofeeder/autofeeder_tests
```
