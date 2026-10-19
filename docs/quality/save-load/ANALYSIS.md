# ANALYSIS — save/load pipeline

Reference map: `world.otbm` 53,252,872 B · 19.0M tiles · 126 blocks ·
53,788 spawn areas · monster XML 10.1 MB / 87,330 creatures · npc XML 137 KB / 1,047
spawn areas. Machine: the dev workstation used for the harness runs; build
`build-vs2022/Release` (MSVC, Release).

## 1. Pipeline map

### Save

```
EditorManager::SaveCurrentMap          editor/managers/editor_manager.cpp:189
  EditorPersistence::saveMap           editor/persistence/editor_persistence.cpp:54
    snapshot map naming state          :66-95     (unnamed map derives aux names)
    rename existing files -> "~"       :102-160   (backup model, see §4)
    MapBackupService::WriteCrashMarker :163        (.saving.txt)
    CreateLoadBar("Saving map...")     :173        (cancellable)
    IOMapOTBM::saveMap                :178
      saveMapToDisk                    io/iomap_otbm.cpp:~690
        writing tiles               0..85%   (writeTileData, per tile chunk)
        finishing file             88%      (flush/close)
        spawns                     91..96%  (OTB5: monster+npc single pass)
        houses                     97%
        zones                      97%
        waypoints                  98%      (XML only when configured)
    rollback on failure/cancel        :187-257  (restore "~", keep marker if restore fails)
    backup rotation / cleanup         :287-340  (ALWAYS_MAKE_BACKUP)
    map.clearChanges()                :~342
```

Tile write: `IOMapOTBM::writeTileData` reports `tilesWritten * 85 / 100` and aborts
when `WasLoadCancelled()`. Compression: `io/compression/gzip_node_file_write_handle.cpp:11`
`gzopen(path, "wb6")`, `gzbuffer(64 KB)` — **gzip level 6**.

### Load

```
EditorManager::LoadMap                 editor/managers/editor_manager.cpp:333
  EditorPersistence::loadMap           editor/persistence/editor_persistence.cpp:29
    CreateLoadBar("Loading <file> (size)")   :50
    Map::open -> IOMapOTBM::loadMapFromDisk  io/iomap_otbm.cpp:~200
      isGzip? decompress 0..5%        :215-221   (Compression::decompressGzipFile, 64 KB chunks)
      loadMapFast                     :~337
        pre-scan nodes                :~425    (5%)
        parallel tile read            :~537    (20..85%, 126 blocks, reports x/y blocks)
        finalize houses/zones         :627     (95%)
      aux XML                       :266-330   (monster/npc, houses, zones, waypoints; named)
      SetLoadDone(100)              :338
  tab + minimap + status            editor_manager.cpp:~400-450
```

### Progress UI

`LoadingManager` (`ui/managers/loading_manager.cpp`) owns both surfaces:
`CreateLoadBar` → `wxGenericProgressDialog` (gauge, Cancel) and `CreateInfoBar` →
app-modal `wxDialog` (no gauge) with a `wxWindowDisabler`. `ScopedLoadingBar` /
`ScopedInfoBar` in `ui/gui.h` are the RAII wrappers. Status: `StatusManager::SetStatusText`
writes **field 0** of a 4-field status bar (`main_frame.cpp:63`); the FPS tick also
writes field 0 → the result line is erased within a frame.

## 2. Measured stages

| operation | before this PR | after this PR |
|---|---|---|
| load total | 6.6–7.1 s | 6.6–7.1 s |
| save total | 18.6–18.7 s | **14.2–14.4 s** |
| tiles (0→85%) | 11.6–11.8 s | 11.3–11.7 s |
| finalize (88%) | ~0.05 s | ~0.05 s |
| spawns (91→96%) | 5.7–6.8 s frozen at 91% | **2.9–3.2 s, 6 steps** |
| houses (97%) | 0.1 s | 0.1 s |
| zones (97%) | 0.1 s | 0.1 s |
| closing/exit | tool frame, off-centre, draggable | app-modal dialog, centred, not draggable |

Spawn internals (same map):

| pass | time | output |
|---|---|---|
| monster only | 2.6–3.1 s | 10,103,410 B / 87,330 creatures |
| npc only | 3.0–3.7 s | 136,889 B / 1,047 areas |
| merged (current) | 2.9–3.2 s | both, byte-identical (SHA256) |

Both passes walked all 53,788 spawn areas and rescanned each radius (avg 2.2, max 30)
→ ~1.4–2.1M tile lookups per pass. The NPC file costs as much as the monster file
despite being 74× smaller: **cost is the scan, not the bytes**.

Determinism check: same input, only the binary changed → monster and npc XML
SHA256 identical (10,103,410 B / 136,889 B). Reachable only after fixing B4
(34 extra empty `<monster>` nodes).

## 3. Bottleneck ranking

| rank | item | cost | share of save | evidence |
|---|---|---|---|---|
| 1 | tile write + gzip level 6 | 11.3–11.7 s | ~81% | stage timestamps; `gzopen "wb6"` |
| 2 | spawn stage (single pass) | 2.9–3.2 s | ~21% | stage logs |
| 3 | load decompress + tile build | 6.6–7.1 s total | — | stage texts |
| 4 | finalize/houses/zones | <0.3 s | ~2% | stage timestamps |

Rank 1 has the largest headroom: ~190 MB of deflate at level 6. Level 1–3 typically
trades 2–5% size for 2–3× speed, which would put the save near 6–8 s without touching
the format (gzip streams are level-agnostic on read). No compression setting exists
today.

Rank 2 further gains need a parallel scan: each spawn area is independent, but the
writer mutates `creature->save()` to avoid duplicates across overlapping areas, so a
threaded scan needs a shared claim map (area index per creature). That is a separate
design, not a quick win.

Rank 3 is dominated by inflate (single-threaded) and the 126-block parallel tile
build; decompression is reported as one 0→5% step, so the dialog looks still for
~1.5 s.

## 4. Safety model today

- Before writing, each existing file is renamed to `<name>~`; `.saving.txt` records
  them (`MapBackupService::WriteCrashMarker`).
- On failure/cancel: `~` files are renamed back; a failed restore keeps the files and
  the marker, and tells the user the paths (good). On success the `~` files are
  rotated to `.<timestamp>.otbm` when `ALWAYS_MAKE_BACKUP` is on, deleted otherwise.
- Startup recovery: `application.cpp:263-301` reads `.saving.txt` and offers to open
  the recovered map.

Gaps:

| gap | consequence |
|---|---|
| canonical file renamed away before write | between rename and success there is no canonical file; recovery depends on `.saving.txt` |
| no fsync of data before close/rename | power loss can keep a zero-length/partial file even with `~` present |
| `ALWAYS_MAKE_BACKUP=true` has no retention | unbounded timestamped copies in the map dir |
| rollback path untested | regressions only visible during a real failure |
| cancel latency unmeasured | no number for M5 |

## 5. Observability gaps

- Result line lives in status field 0 and is overwritten by the FPS tick (B5).
- Tiles stage is not logged with elapsed ms (only visible in the dialog).
- No throughput/ETA (`MB/s`, `tiles/s`) in the dialog.
- Load decompression is one step, no byte progress.
- Stage log lines are per-file for spawns only; houses/zones have none.

## 6. Risks

- **Peak memory** — the merged spawn writer holds two DOMs at once (monster + npc).
  The 10 MB XML is a much larger DOM; this must be measured before we keep it (V9).
  If the budget is exceeded, the fallback is a streaming writer, which also improves
  the rank-2 cost.
- **Byte-stability tests** — changing gzip level changes `world.otbm` bytes. Tests must
  compare semantic content or pin the level; sidecar XML must stay byte-stable (V5).
- **Atomic replace portability** — `std::filesystem::rename` does not replace an
  existing target on Windows; need `MoveFileExW(REPLACE_EXISTING)` /
  `ReplaceFileW`, POSIX `rename` is already atomic.
- **Network/read-only dirs** — `~` rename and temp-file strategies fail differently;
  the error path must stay explicit (it already reports failed restores).

## 7. Evidence log

| date | claim | method |
|---|---|---|
| 2026-10-18 | duplicate pane assert gone; one palette restored | run + `AddPane` assert counter (0 hits) |
| 2026-10-18 | closing window centred on editor | dialog rect 2722,472–3078,547 vs frame 2100,60–3700,960 |
| 2026-10-19 | editor cannot be dragged during save/close | real mouse drag, frame rect unchanged 2100,60–3700,960 |
| 2026-10-19 | spawn stage 6.8 s frozen → 2.9–3.2 s moving | dialog text sampling @20–25 ms, stage logs |
| 2026-10-19 | single pass output identical | same input, two binaries, SHA256 of both files |
| 2026-10-19 | load names sidecars, summary correct | dialog texts + `Map loaded: … (18997668 tiles) in 6.6 s` |

## 8. Follow-ups (out of scope, separate specs)

- Parallel spawn-area scan with a shared claim map (rank 2, expected ×4–8).
- Block-parallel gzip (rank 1 + rank 3): save and load both gain.
- Streaming XML writers to remove the DOM (memory + speed).
