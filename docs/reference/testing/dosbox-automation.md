# dosbox-automation recipe

The original game can be launched, driven by injected keys, photographed at
native resolution and shut down with **zero manual input**, over the REST API
of dosbox-automation. Verified 2026-10-05 on Windows 10 with dosbox-automation
0.85.1 and PowerShell 5.1. Install and config are in
[DOSBox setup](dosbox-setup.md).

The loop is closed: send keys, read the frame, decide the next step.

## Starting a run

From the project root, with the game copied to `build\dosbox\game` (see the
setup page):

```powershell
$dosbox = 'tools\dosbox-automation\dosbox-automation-0.85.1\dosbox.exe'

# Every API request has to carry this token.
$bytes = New-Object byte[] 32
[Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
$env:DOSBOX_API_TOKEN = -join ($bytes | ForEach-Object { $_.ToString('x2') })

$p = Start-Process $dosbox -PassThru -ArgumentList '--noprimaryconf','--nolocalconf','--conf','docs\reference\testing\minebombers.dosbox.conf'

$api = 'http://127.0.0.1:8386/api/v1'
$h = @{ Authorization = "Bearer $env:DOSBOX_API_TOKEN" }

# Wait for the API. It answers within a second.
foreach ($i in 1..60) {
    try { Invoke-RestMethod "$api/status" -Headers $h -TimeoutSec 2 | Out-Null; break }
    catch { Start-Sleep -Milliseconds 500 }
}
```

The emulator takes the token from `DOSBOX_API_TOKEN` at startup. If the
variable is not set it makes up a token of its own and logs only the first
characters: the game runs, but every request is answered with 401.

The title screen is up about two seconds after the API answers.

## Calls

All paths are under `http://127.0.0.1:8386/api/v1`.

| Call | Purpose |
|---|---|
| `GET /status` | Running state and current program, e.g. `"program":"MB"`, `"canonical_name":"C:\\MB.EXE"` |
| `GET /video/frame?format=png&mode=raw` | Current frame as a PNG at the native 640x480. `mode=rendered` gives the scaled window image instead |
| `POST /input/sequence` | Timed key events: `{"events":[{"type":"key","key":"KBD_enter","pressed":true,"t":0}, ...]}`, `t` in ms from the start of the sequence |
| `POST /input/type` | Type text: `{"text":"...","cps":15}` |
| `POST /dosbox/shutdown` | Quit the emulator |

A screenshot, two key taps and a shutdown:

```powershell
function Shot($name) {
    Invoke-WebRequest "$api/video/frame?format=png&mode=raw" -Headers $h -OutFile "build\dosbox\$name.png" -UseBasicParsing
}

# Key down at t = 0, up at t = $ms. A longer $ms holds the key.
function Tap($key, $ms = 80) {
    $events = @(
        @{ type = 'key'; key = "KBD_$key"; pressed = $true;  t = 0 },
        @{ type = 'key'; key = "KBD_$key"; pressed = $false; t = $ms })
    Invoke-RestMethod "$api/input/sequence" -Method Post -Headers $h -ContentType 'application/json' -Body (@{ events = $events } | ConvertTo-Json -Depth 4) | Out-Null
}

Start-Sleep -Seconds 3
Shot title
Tap enter                  # title -> main menu
Start-Sleep -Seconds 3     # let the fade finish
Tap down                   # cursor from New game to Options
Start-Sleep -Milliseconds 500
Shot main-menu

Invoke-RestMethod "$api/dosbox/shutdown" -Method Post -Headers $h | Out-Null
$p.WaitForExit(10000)
```

One sequence can hold several taps: give each event its own `t`. Two taps
250 ms apart are a press and a release at 0 and 80, then at 250 and 330.

## Key names

Keys are the emulator's `KBD_` names. An unknown name is refused with
`400 Unknown key`, which makes a name cheap to check. Names used with this
game:

- `KBD_enter`, `KBD_esc`, `KBD_up`, `KBD_down`, `KBD_left`, `KBD_right`
- the numpad, which holds player 1's default controls: `KBD_kp1` .. `KBD_kp9`
- `KBD_pagedown`, `KBD_pageup`, `KBD_tab`, `KBD_grave`
- letters `KBD_a` .. `KBD_z`, digits `KBD_0` .. `KBD_9`, function keys
  `KBD_f1` .. `KBD_f12`

## More of the API

The emulator serves a Swagger UI with the full endpoint list at
`http://127.0.0.1:8386`. Endpoints likely to be useful here that this recipe
has not exercised:

| Endpoint | Purpose |
|---|---|
| `GET /memory/:segment/:offset/:len` | Read emulated memory |
| `POST /capture/video/start`, `/capture/video/stop` | ZMBV video capture |
| `POST /script/load`, `/script/start` | Run a sandboxed Lua script on the emulation thread, for frame-accurate waits and key presses |

## Limits

- Injected input is paced in wall-clock milliseconds, so a busy host can
  shift where a key lands. Keep a margin around palette fades, which swallow
  keys (see the quirks in the setup doc).
- No pause or single-frame-step endpoint was found. Use a Lua script when a
  capture has to land on an exact frame.
- Runs are not bit-reproducible by default: the game seeds its RNG from the
  clock.
- The emulator opens a visible window. A second `--conf` can move it to
  another monitor and mute it (see the setup doc).

## Earlier recipe

Before 2026-10-05 this page described a DOSBox-X rig: AUTOTYPE keystrokes on
a fixed pace, `dx-capture` video, and ffmpeg to pull frames out of it. It
worked, but open-loop only, with no way to look at the screen before the next
key. That version of the page is in the git history.
