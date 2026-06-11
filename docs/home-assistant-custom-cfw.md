# Home Assistant Control

This Custom Prusa xBuddy Firmware adds a small `PrusaLink` command endpoint for light and fan control through Home Assistant.

## Endpoint

Send a `POST` request to:

```text
/api/printer/command
```

With JSON body:

```json
{
  "command": "M152 L2 R255 G255 B255 D0"
}
```

Only `M150`, `M151`, `M152`, and `M153` are accepted by this endpoint.

## Supported Commands

### Display / Status LEDs

```text
M150 R255 G128 B0
```

### Side Strip LEDs

```text
M151 R255 G128 B0 D0 T100
```

### Targeted Light Control

`M152` supports separate light targets:

- `L0` display / status LEDs
- `L1` bed LEDs
- `L2` chamber LEDs

Examples:

```text
M152 L0 R255 G128 B0
M152 L1 R255 G255 B255 D0
M152 L2 R255 G255 B255 D0
```

### Fan Control

`M153` supports fan control for xBuddy Extension based printers:

- `T0` chamber cooling fans
- `T1` filtration fan
- `S-1` auto mode
- `S0` to `S100` fixed speed in percent

Examples:

```text
M153 T0 S-1
M153 T0 S40
M153 T1 S75
M153 T1 S0
```

## Home Assistant Example

Example `rest_command` entries:

```yaml
rest_command:
  prusa_display_light_orange:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M152 L0 R255 G128 B0"}'

  prusa_bed_light_on:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M152 L1 R255 G255 B255 D0"}'

  prusa_chamber_light_off:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M152 L2 R0 G0 B0 D0"}'

  prusa_chamber_fans_auto:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M153 T0 S-1"}'

  prusa_chamber_fans_50:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M153 T0 S50"}'

  prusa_filtration_fan_70:
    url: "http://PRINTER-IP/api/printer/command"
    method: post
    headers:
      X-Api-Key: !secret prusa_api_key
      Content-Type: application/json
    payload: '{"command":"M153 T1 S70"}'
```

## Notes

- `D0` can be used for persistent light overrides where supported.
- `M153` is intended for manual control and testing from Home Assistant automations.
- For regular printer-side cooling logic, use `S-1` to return fans to automatic mode.
