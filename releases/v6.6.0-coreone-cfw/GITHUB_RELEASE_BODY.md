## N3DP Custom xBuddy Firmware v6.6.0 Core One

Custom firmware build based on official Prusa-Firmware-Buddy `v6.6.0`.

### Build

- Printer: Prusa CORE One
- Preset: `coreone_release_boot`
- Firmware version in BBF: `6.6.0+15529`
- Bootloader dependency for Core One: `2.5.0`
- Release asset: `N3DP_CUSTOM_XBUDDY_FW_V6_6_0_COREONE_BOOT.BBF`

### Included CFW features

- Custom theme presets and accent-color handling
- TriForce One theme preset
- Custom vent position presets, calibration UI, and safe movement path
- Custom nozzle cleaning / wiper options and test action
- Chamber/display/bed light G-code control hooks for Home Assistant workflows
- Core One autofeeder support

### Notes

This is an independent custom firmware release and is not an official Prusa Research firmware build.
Flash only if you understand the risk of custom firmware on your printer.

Use the BOOT BBF asset for testing this release. The previous NOBOOT-style package was avoided here because the printer had shown bootloader-stage issues during testing.
