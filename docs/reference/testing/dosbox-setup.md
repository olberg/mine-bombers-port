# DOSBox setup for fidelity audits

Fidelity audits compare the port side by side against the original game running in DOSBox. This document is the reproducible recipe. You must supply your own copy of the original game (it is not distributed with this repository); place it in an `original/` directory at the project root.

## Install

Use **DOSBox-X** (not stock DOSBox). DOSBox-X has better VGA accuracy, a built-in screenshot key, and video capture support.

- Windows: download the latest release from <https://dosbox-x.com>. Extract anywhere; the binary is `dosbox-x.exe`.

Stock DOSBox 0.74-3 also works for screenshots, but some VGA edge cases render differently. Prefer DOSBox-X.

## Launching the original

From the project root:

```
dosbox-x.exe -conf docs/reference/testing/minebombers.dosbox.conf original/MB.EXE
```

The config file is committed alongside this doc and pins:

- CPU: `core=auto`, `cycles=fixed 50000` (approximates a 486DX2/66, matches original target)
- Machine: `machine=svga_s3` (full VGA Mode 12h support)
- Scaler: `scaler=none` (no smoothing — we want raw VGA output)
- Sound Blaster: `sbtype=sb16`, `oplmode=opl3` (matches original's `AT OPL3` mode)
- Output: `output=openglpp` (pixel-perfect scaling)
- Frame limit: 70 Hz reported, 60 Hz real — close enough for audits that don't measure audio timing directly
- Autoexec: mounts `original/` as `C:` and runs `MB.EXE`

## Capturing screenshots

DOSBox-X: **Ctrl-F5** saves a PNG to `<capture dir>`. The default capture dir is `%APPDATA%\DOSBox-X\capture`. The committed conf overrides it with `captures=captures`.

Screenshots are saved at the **native VGA resolution** (usually 640x480 for this game), not the window size. This is what we want — do not upscale on capture; scale at comparison time if needed.

## Capturing video clips

DOSBox-X: **Ctrl-Alt-F5** starts/stops video capture (saves as `.avi` to the captures dir). Use this for timing-sensitive audits: movement speed, animation rates, explosion propagation, music sync.

## Reproducibility checklist

When capturing reference screenshots, follow this order so every frame is deterministic:

1. Cold-start DOSBox-X with the committed conf. **Do not keep it open between captures** — boot state matters for RNG.
2. At the `C:\>` prompt, type `MB.EXE` and Enter. Do not press any extra keys before this.
3. Wait for the title screen to fully fade in before taking the first screenshot.
4. For in-game captures, go through the same menu path each time (new-player → default options → known map). Document the sequence in a sibling `.notes.txt`.
5. If audio is part of the audit, ensure DOSBox-X's Sound Blaster mixer is at 100% and the host mixer is unmuted.

## What the port needs to match

- **Pixel output**: native 640x480 is the unit of comparison.
- **Palette**: 16 colors, defined at boot. The port loads the same palette from the SPY file headers. Palette exactness is a hard gate.
- **Audio events**: captured via the video recording; SFX onset timestamps can be extracted from the recording and compared.
- **Timing**: DOSBox at `cycles=fixed 50000` approximates the original's 486DX2/66 target. The game uses its own frame loop, not wall-clock time, so the port matching "update rate" (60 FPS logical) is more important than matching CPU cycles.

## Known DOSBox quirks

- **Audio crackling** at low cycle counts. If audio is being audited, bump cycles to `max` temporarily.
- **Screenshot of a fading frame** may capture a transient palette — retry if the PNG looks washed out.
- **Key-repeat** in DOSBox-X follows the host OS, not the original's INT 9 ISR. Menu cursor auto-repeat timing therefore cannot be audited from DOSBox; read it from the decompiled source (seg_1008 keyboard handler) instead.

## File layout

```
original/                   # your copy of the original game (not distributed)
captures/                   # DOSBox-X screenshots and clips land here
docs/reference/testing/
  dosbox-setup.md           # this file
  minebombers.dosbox.conf   # committed DOSBox-X conf
```
