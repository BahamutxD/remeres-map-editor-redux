# VERIFY — save/load pipeline

Only evidence goes here. A claim without a number, hash or screenshot is not evidence.
Every row cites the method used.

## Reference setup

- Build: `build-vs2022/Release` (MSVC Release). The app reads `config.toml` **from the
  working directory**, and finds its assets relative to the executable, so run it from
  `build-vs2022/Release`.
- Reference map: `C:\Users\user\AppData\Local\Temp\opencode\mapref\world.otbm`
  (copy of the user's `world.otbm`, 53,252,872 B) + its sidecar XMLs.
  Never save into the user's server directory (C8).
- Harness: `C:\Users\user\.config\opencode\skills\redux-harness\scripts\RmeAuto.ps1`
  (`[Rme]::GetMain`, `ByTitle`, `Shot`, `Dismiss`, `PostMessage`, `SetWindowPos`).
- Menu command: `WM_COMMAND 0x0111`, `wParam = 6003` = Save
  (`MAIN_FRAME_MENU 6001 + ActionID`).

## Recipes

### 1. Load and wait until the map is ready

```powershell
$p = Start-Process rme.exe -WorkingDirectory $bd -ArgumentList "`"$map`"" -PassThru `
     -RedirectStandardOutput "$bd\run.log" -RedirectStandardError "$bd\run.err"
$main = [IntPtr]::Zero
while ((Get-Date) -lt $deadline) {
  $m = [Rme]::GetMain([uint32]$p.Id); if ($m -ne [IntPtr]::Zero) { $main = $m }
  if ($m -ne [IntPtr]::Zero -and [Rme]::Title($m) -match '\.otbm') { break }
  [void][Rme]::Dismiss([uint32]$p.Id)     # dismiss loader dialogs/warnings
  Start-Sleep -Milliseconds 250
}
```

Title contains the map name only after a successful load — use it, not a timer.

### 2. Capture dialog text (progress / stage names)

```powershell
# EnumWindows + EnumChildWindows, GetWindowText on visible children
$d = [Dlg]::ByTitle([uint32]$p.Id, 'Saving')
foreach ($t in [Dlg]::Texts($d)) { $t }
```

Poll every 20–40 ms to catch short stages. `wxGenericProgressDialog` exposes its
message as a child static text; screenshots (`[Rme]::Shot`) are the fallback.

### 3. Save and time it

```powershell
[void][Rme]::PostMessage($main, 0x0111, [IntPtr]6003, [IntPtr]::Zero)
# wait until a dialog titled 'Saving' appears, then until it disappears
```

Measure from the post to the dialog disappearing. **Do not** break early: a modal
dialog swallows `WM_COMMAND`, so posting during a load silently does nothing (this
produced an invalid byte-comparison run once).

### 4. Byte-identical comparison (determinism)

The binary must live in `build-vs2022/Release` (assets are resolved next to it).

1. keep the previous binary as `Release\rme-before.exe`;
2. fresh copy of the reference map for each run;
3. save with each binary, same input bytes;
4. `Get-FileHash -Algorithm SHA256` on every sidecar XML.

Compare multi-MB XML by hash, not by eye. A 2 KB delta in a 10 MB file is invisible
in a diff preview but is a behaviour change (see B4).

### 5. Drag test (modal input block)

```powershell
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;
public class Mouse { [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
[DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,IntPtr e); }'
# caption centre -> left down -> move +320,+180 -> left up
```

Assert the frame rect (e.g. `2100,60 3700,960`) is unchanged after the drag.

### 6. Status bar / result line

Status field 0 is overwritten by the FPS tick within a frame. Screenshot immediately or
read the log line instead (`Map saved: …`, `Map loaded: …`). Crop the status strip with
`Graphics.CopyFromScreen` to keep the image small.

### 7. Peak memory

```powershell
$p = Get-Process -Id $pid; $p.PeakWorkingSet64   # after the save completes
```

Read after the save, before closing, on a freshly loaded map (same state each run).

### 8. Crash / cancel

- **Cancel**: post the save, then post `WM_COMMAND` to the dialog's Cancel button or
  send `BM_CLICK` to it; assert `Cancelling...` within 250 ms, then the canonical file
  parses and no `.tmp`/`~` remain.
- **Kill**: `Stop-Process -Id $pid -Force` mid-write; then load the canonical file and
  assert it is the previous generation. `~` + `.saving.txt` may remain (expected on
  the rename-away model, must not on the temp+replace model after T5).

## Evidence log

| date | metric | baseline | now | method |
|---|---|---|---|---|
| 2026-10-19 | M2 spawn stage | 6.8 s frozen | 2.9–3.2 s moving | dialog text @25 ms + stage log |
| 2026-10-19 | M1 save total | 18.7 s | 14.2–14.4 s | post → dialog gone |
| 2026-10-19 | M3 load total | 6.6–7.1 s | 6.6–7.1 s | `Map loaded` log line |
| 2026-10-19 | M7 spawn XML determinism | unknown | identical | SHA256, same input, two binaries |
| 2026-10-18 | M4 closing dialog centred | off-monitor | centred, 2722,472–3078,547 | window rects |
| 2026-10-19 | V4 not draggable | yes (tool frame) | no | real mouse drag |
| 2026-10-19 | M6 partial canonical file | possible | **unmeasured** (T5) | kill mid-write |
| 2026-10-19 | M9 peak memory | **unmeasured** | — | `PeakWorkingSet64` (T13) |
| 2026-10-19 | M5 cancel latency | **unmeasured** | — | cancel click (T11) |

## Pitfalls seen during this work

- Modal dialog (load/save) swallows `WM_COMMAND`; posting a save during a load does
  nothing and looks like a fast save.
- Copying `rme.exe` outside `Release` breaks asset/version resolution → map load fails.
- Killing a process mid-save leaves `world.otbm~` + partial XML; the map copy must be
  discarded and re-created from the source.
- `wxGenericProgressDialog` repaints per `Update()`; a stage with one percent for
  6.8 s means the writer does not report, not that it is hung.
- Status field 0 is shared with the FPS tick; result lines are ephemeral.
- Two monitors: the editor may be on DISPLAY2 (1920..3840); centre checks must compare
  against the frame rect, not the primary screen.
