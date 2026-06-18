# Release Notes

## N3DP Custom xBuddy Firmware v6.5.7

Updated the custom CORE One firmware branch from the previous custom baseline to upstream `Prusa-Firmware-Buddy` tag `v6.5.7`.

## Build Result

- Configure preset: `coreone_release_noboot`
- Build directory: `build-vscode-buddy`
- Firmware artifact: `N3DP_CUSTOM_XBUDDY_FW_V6_5_7_COREONE_NOBOOT.BBF`
- Reported firmware version: `6.5.7+57`
- Printer target: `COREONE`

## Carried Forward

- custom CFW release and licensing documentation
- custom GUI/theme color baseline
- CORE One autofeeder integration
- `M8600` custom G-code registration
- Home Assistant example release material

## Verification

- `cmake --preset coreone_release_noboot`
- `cmake --build build-vscode-buddy --parallel`
- `git diff --check`
- exact conflict-marker scan after resolving cherry-pick conflicts

The first build attempt found a `v6.5.7` callback API change in `gui::knob`; `src/gui/guimain.cpp` was updated to match the new callback signatures and the build then completed.
