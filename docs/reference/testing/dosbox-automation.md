# DOSBox-X automation recipe

**Verdict: feasible.** The original game can be launched, driven by scripted
keystrokes, captured (lossless video + PCM audio), and shut down with **zero
manual input**. Verified 2026-06-10 on Windows 10 with DOSBox-X 2026.03.29
mingw64.

Key feasibility results:

| Question | Answer |
|---|---|
| Does AUTOTYPE input reach MB.EXE's own INT 9 keyboard ISR? | **Yes** — injection is at the emulated-keyboard level, below the BIOS buffer. Injected ESC/Enter/F10 navigated title → menu → player select → shop → rounds. |
| Can DOSBox-X start and exit unattended? | Yes — `shutdown /s` in `[autoexec]` + `quit warning=false` + `startbanner=false` + `-fastlaunch`. A pure-DOS smoke run completes in ~2 s. |
| Video/audio capture without hotkeys? | Yes — `dx-capture MB.EXE` records AVI (ZMBV lossless, 640×480) with PCM s16le audio for the program's whole lifetime. |
| Screenshots? | Extract frames from the AVI with ffmpeg (`-vf fps=...`). No in-shell screenshot command exists; the Ctrl+F5 hotkey is not scriptable. |

## Setup

1. Download a DOSBox-X Windows **mingw64 portable** release and unzip under
   `tools/dosbox-x/` (gitignored). The exe lands at
   `tools/dosbox-x/<ver>/mingw-build/mingw/dosbox-x.exe`.
2. **TRAP — do not use an `-osfree` release.** Releases tagged `…-osfree`
   (which can be the *latest* release on GitHub) are built from the
   `main-osfree` branch with the built-in DOS **removed**: no `mount`, no
   `shutdown`, autoexec commands fail with `Bad command or filename` and the
   machine ends at "Operating System Not Found". Use a plain release
   (e.g. `dosbox-x-v2026.03.29`, asset `dosbox-x-mingw64-2026.03.29-portable.zip`).
3. Copy `original/` to a scratch dir (e.g. `build/dosbox-spike/game/`) and
   mount **that** — the game writes .DAT files next to MB.EXE.

## Working config template

```ini
[sdl]
autolock=false
quit warning=false

[dosbox]
captures=<project>\build\dosbox-spike\capture
memsize=16
startbanner=false
fastbioslogo=true

[cpu]
core=dynamic
cycles=max

[autoexec]
mount c "<project>\build\dosbox-spike\game"
c:
autotype -w 15 -p 2.0 esc enter esc esc esc esc enter f10 enter
dx-capture MB.EXE
echo GAME-EXITED-OK > MARKER.LOG
shutdown /s
```

Launch (PowerShell — note the timeout/kill guard; a key-script that strands
the game on a key-wait screen hangs the run):

```powershell
$exe = 'tools\dosbox-x\v2026.03.29\mingw-build\mingw\dosbox-x.exe'
$p = Start-Process $exe -WorkingDirectory (Split-Path $exe) `
     -ArgumentList '-conf','<conf>','-fastlaunch','-nogui' -PassThru
if (-not $p.WaitForExit(120000)) { $p.Kill() }
```

Success detection: the `echo … > MARKER.LOG` line after the game command
only runs if the game exited, so the marker file = "key script drove the
game back to DOS".

## AUTOTYPE notes

- Syntax: `autotype [-w initial_wait_s] [-p pace_s] key1 key2 …`. Key names:
  `esc`, `enter`, `f10`, letters/digits as-is. It queues keystrokes
  asynchronously; put it **before** the game command.
- It is **fire-and-forget with a fixed pace** — no conditional waits, no
  state detection. Screens that wait for a key (title, match-winner/results
  screen) consume one keystroke each; palette fades (7 steps) and level
  loads delay state changes, so use a generous pace (≥ 2 s) and expect to
  iterate against captured video.
- Observed traversal with the template above (2 defaults players): title →
  main menu → PLAY → player select → **shop** → round ↔ shop loop via ESC →
  match end → **winner screen (waits for a key — budget keystrokes for
  it)** → main menu → F10 quits.
- MB.EXE hooks INT 9 directly; AUTOTYPE still works (it injects scancodes
  at the emulated keyboard controller, not the BIOS buffer).

## Capture and analysis

- `dx-capture MB.EXE` writes `mb_NNN.avi` into the `captures=` dir; a new
  file starts on each video-mode change (the tiny `mb_000.avi` is the text
  mode prelude).
- Codec: ZMBV (lossless) 640×480 + PCM s16le audio — pixel-exact frames and
  raw audio, suitable for palette comparison and music-schedule
  verification (e.g. interest-rounding boundaries, music order jumps).
- Frames: `ffmpeg -i mb_001.avi -vf fps=1/10 out_%02d.png`.
- Audio: `ffmpeg -i mb_001.avi -vn audio.wav`.

## Limitations

- No conditional logic: choreographies must be timed open-loop. For long
  experiments, prefer sequences that are state-insensitive (repeated ESC) or
  end in an idle state, and verify by reviewing the AVI.
- **No config file ships** — the game uses factory defaults (cash 750,
  15 rounds) until the options menu writes **`OPTIONS.CFG`** (17 bytes,
  format in `file-formats.md`) next to MB.EXE on options exit (a file
  named "ASETUK.DAT" is ignored by the game).
  To vary options, pre-write `OPTIONS.CFG` into the scratch dir.
- **Capture numbering continues across runs** in the same `captures=`
  dir, and a killed run's ZMBV AVI is still readable — wipe or rename
  the captures dir between runs or you WILL mis-attribute footage
  (this once cost an hour).
- DOSBox-X's DOS shell processes `>` redirects **inside `rem` lines** —
  keep redirect arrows out of autoexec comments (stray 0-byte files).
- ffmpeg on ZMBV: use output-side seek (`-i file -ss N`); input-side
  seek lands on non-keyframes and fails to decode.
- `cycles=max` runs the game loop UNTHROTTLED (the original's pacing is
  a calibrated TP7 Delay()-style loop that fast CPUs defeat). PIT-tick
  timing (e.g. the time limit) stays wall-clock-correct, but frame-tied
  timing comparisons need a calibrated fixed-cycles run.
- DOSBox-X runs a visible window (no true headless mode used here);
  `-silent` exists but its interaction with `dx-capture` is unverified.
