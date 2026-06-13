# Independent Release Guide

This repository can be published as its own project and does not need to be a visible GitHub fork.

GitHub fork status and software license obligations are separate things:

- You may publish this code from a new repository with its own name, branch structure, and releases.
- If the code is derived from `Prusa-Firmware-Buddy`, the firmware code remains subject to GPL-3.0 obligations.
- If upstream graphics, icons, themes, or other design assets are reused, their original license terms still apply.

## What This Means In Practice

You can:

- publish from your own repository instead of a GitHub fork
- use your own release names and your own versioning
- document your own build instructions and feature set
- replace upstream branding and assets over time

You still need to:

- keep the GPL source code obligations for derived firmware code
- preserve upstream notices where required
- ship corresponding source code when distributing public binaries
- respect the non-code asset licenses listed in `LICENSE.md`

## Recommended Repo Setup

For an independent public release, keep these files in the repository root:

- `LICENSE.md`
- `NOTICE`
- `README.md`
- this document: `doc/independent-release.md`

## Recommended README Wording

Use wording that makes the project identity clear without hiding the origin:

> This repository is an independent firmware project based on Prusa-Firmware-Buddy.
> It is maintained separately and is not an official Prusa repository.

## Recommended Release Rules

For every public firmware release:

1. Publish the matching source code commit in the same repository.
2. Keep `LICENSE.md` and `NOTICE` in the repository.
3. Mention in release notes whether upstream graphics or original assets are still included.
4. Avoid presenting the release as official Prusa firmware.

## Strong Recommendation For A Cleaner Independent Identity

If you want to move further away from the appearance of a modified upstream release:

- use your own project name
- replace custom UI branding and artwork with your own assets
- keep compatibility wording factual
- avoid upstream trademarks in the primary product name

Example:

- Good: `N3DP Custom xBuddy Firmware for Prusa Core One`
- Riskier: `Official Prusa Core One N3DP Firmware`

## Suggested Release Note Snippet

```text
This is an independent community firmware build based on Prusa-Firmware-Buddy.
Source code for this release is available in this repository.
Firmware code remains subject to GPL-3.0. Non-code assets may be subject to
additional licenses documented in LICENSE.md.
This is not an official Prusa release.
```

## Final Note

This document is a practical release policy for this repository.
It is not legal advice.
