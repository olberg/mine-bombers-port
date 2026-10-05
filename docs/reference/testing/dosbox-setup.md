# DOSBox setup for fidelity audits

Fidelity audits compare the port against the original game running in an emulator. This document is the reproducible recipe. You must supply your own copy of the original game (it is not distributed with this repository); place it in an `original/` directory at the project root.

The emulator is **dosbox-automation**, a DOSBox Staging fork with an HTTP REST API. Keys are injected and frames are read back over HTTP, so a capture run needs no manual input; the [dosbox-automation recipe](dosbox-automation.md) covers that part. Verified 2026-10-05 on Windows 10 with dosbox-automation 0.85.1.

## Install

1. Download the Windows portable zip from the releases page, <https://github.com/dosbox-automation/dosbox-automation/releases>. This recipe was verified with `dosbox-automation-0.85.1-7567ec4-windows-x64.zip`. Check its SHA-256 against the release's `SHA256SUMS`.
2. Unzip it anywhere. The examples assume `tools/dosbox-automation/<version>/dosbox.exe` under the project root; `tools/` is git-ignored.

Nothing else needs installing. Started with `--noprimaryconf --nolocalconf`, the emulator reads no per-user or working-directory config, only the files named on its command line.

## Launching the original

MB.EXE writes its `.DAT` and `.CFG` files next to itself, so run it from a copy of `original/`. From the project root, in PowerShell:

```powershell
New-Item -ItemType Directory -Force build\dosbox | Out-Null
Copy-Item original build\dosbox\game -Recurse

$dosbox = 'tools\dosbox-automation\dosbox-automation-0.85.1\dosbox.exe'
Start-Process $dosbox -ArgumentList '--noprimaryconf','--nolocalconf','--conf','docs\reference\testing\minebombers.dosbox.conf'
```

The game is at its title screen a couple of seconds later.

The config file `minebombers.dosbox.conf` is committed alongside this doc. It is short:

- `[autoexec]` mounts `build\dosbox\game` as `C:`, runs `MB.EXE`, and exits the emulator when the game quits. The mount path is relative, which is why the emulator is started from the project root.
- `[webserver]` turns on the REST API at `http://127.0.0.1:8386`.
- `startup_verbosity = quiet` skips the emulator's startup banner.

Everything else is the emulator's default: an S3 SVGA machine, 16 MB of memory, a Sound Blaster 16 at A220 I7 D1. CPU speed (`cpu_cycles`, `cpu_cycles_protected`) is not pinned either; pin it before a timing-sensitive audit. The DOSBox-X conf this page used before ran at a fixed 50000 cycles.

To put the window on another monitor or to mute it, pass a second `--conf` after the first. Its settings are layered on top:

```ini
[sdl]
display = 1

[mixer]
nosound = on
```

`display` counts from 0, the primary monitor.

Started like this the emulator is an ordinary DOSBox and the game can be played in its window. For a scripted run, set an API token before starting it; see the [recipe](dosbox-automation.md).

## Capturing screenshots

Read the current frame over the API (`GET /video/frame?format=png&mode=raw`; the recipe has the full command). The PNG comes from the emulator's frame buffer at the **native VGA resolution**, 640x480 for this game, whatever the window size. This is what we want — do not upscale on capture; scale at comparison time if needed.

## Capturing video

The API can record lossless ZMBV video (`POST /capture/video/start`, `POST /capture/video/stop`). This recipe has not exercised it. When extracting frames from a ZMBV file with ffmpeg, seek on the output side (`-i file -ss N`); input-side seek lands on non-keyframes and fails to decode.

## Reproducibility checklist

When capturing reference screenshots, follow this order so every frame is deterministic:

1. Cold-start the emulator for every capture run. **Do not keep it open between captures** — boot state matters for RNG.
2. Wait for a screen to finish fading in before taking its screenshot. A frame caught mid-fade has a transient palette; retake it.
3. For in-game captures, go through the same menu path each time (new-player → default options → known map). Document the sequence in a sibling `.notes.txt`.

## What the port needs to match

- **Pixel output**: native 640x480 is the unit of comparison.
- **Palette**: 16 colors, defined at boot. The port loads the same palette from the SPY file headers. Palette exactness is a hard gate.
- **Audio events**: SFX onset timestamps can be extracted from a video recording and compared.
- **Timing**: the game paces its own frame loop with a calibrated delay loop, so the emulated CPU speed changes how fast frames go by. PIT-tick timing, such as the round time limit, stays wall-clock-correct. Pin the CPU speed before comparing anything tied to frames.

## Game-side quirks

- **Fades eat keys.** A key pressed during a palette fade is lost. Leave about 1.5-2 s after a transition and confirm the screen with a screenshot before sending the next key.
- **No config file ships with the game.** It runs on factory defaults (cash 750, 15 rounds) until the options screen writes `OPTIONS.CFG` (17 bytes, format in [File Formats](../formats/file-formats.md)). To set options for a run, write that file into the game copy before starting.
- **MB.EXE hooks INT 9 directly.** Injected keys still reach it.
- **Key repeat** comes from the host or the injection timing, not from the original's INT 9 ISR. Menu cursor auto-repeat timing therefore cannot be audited from the emulator; read it from the decompiled source (seg_1008 keyboard handler) instead.

## File layout

```
original/                     # your copy of the original game (not distributed)
tools/dosbox-automation/      # the emulator (git-ignored)
build/dosbox/                 # scratch for a run (git-ignored)
  game/                       # working copy of original/
docs/reference/testing/
  dosbox-setup.md             # this file
  dosbox-automation.md        # driving the game over the REST API
  minebombers.dosbox.conf     # committed emulator conf
```
