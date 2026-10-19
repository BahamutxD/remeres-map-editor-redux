# SPEC — save/load pipeline quality

Scope: editor persistence (`source/editor/persistence`, `source/io`,
`source/ui/managers/loading_manager`), progress UI, backups, crash recovery.
Format: see `FORMAT.md`. Long form: `docs/quality/save-load/`.

## §G goal

Save/load is trustworthy, observable, fast: no data loss on cancel or crash, every
stage names its file with real progress, and a 53 MB / 19M-tile map saves in ≤ 8 s
and loads in ≤ 7 s on the reference machine.

## §C constraints

- C1 no change to on-disk formats (OTBM, spawns/houses/waypoints/zones XML)
- C2 gzip output must stay readable by the same clients; level may change
- C3 cancel must stay available during save
- C4 Windows/macOS/Linux; `std::filesystem` rename semantics per platform
- C5 no new dependency for progress/UI; reuse `LoadingManager`
- C6 editor window never movable/usable while operation runs
- C7 sidecar XML determinism: same map + settings → identical bytes
- C8 user's map directory must never be written by tests (use copies)

## §I interfaces

- I.save `EditorPersistence::saveMap(Editor&, FileName, bool showdialog)`
- I.io-save `IOMapOTBM::saveMapToDisk(Map&, FileName)` → tiles, finalize, aux
- I.io-load `IOMapOTBM::loadMapFromDisk(Map&, FileName)` → decompress, scan, tiles, aux
- I.xml `MapXMLIO::saveSpawns/saveHouses/saveWaypoints/saveZones`
- I.progress `LoadingManager` (`CreateLoadBar`, `CreateInfoBar`, `SetLoadDone`, `SetLoadText`, `PollCancellation`)
- I.backup `EditorPersistence` `~` rename model + `MapBackupService::WriteCrashMarker/.saving.txt`
- I.result status bar + `spdlog` lines with file, size, ms

## §V invariants

- V1 canonical map file never partial: write temp in target dir, then atomic replace
- V2 cancel/fail restores previous generation; when restore fails keep `~` + crash marker and tell user path
- V3 progress monotonic and named; long stage must show a change at least every 1 s
- V4 operation modal: editor not usable, window not movable
- V5 aux XML save deterministic: identical input → identical bytes
- V6 spawn stage scans spawn list once (no per-file pass duplication)
- V7 load names every sidecar file it reads (spawns/houses/zones/waypoints)
- V8 every save/load ends with result line: file, size, elapsed, in log and status
- V9 save peak working set stays within the recorded budget; raising it needs a streaming writer
- V10 cancel honored ≤ 250 ms and leaves no `~` on success
- V11 gzip level is configurable; default choice keeps save ≤ 8 s on reference map

## §T tasks

| id | status | task | cites |
|---|---|---|---|
| T1 | x | unify closing/loading/saving dialogs, app-modal + hourglass | V4!I.progress |
| T2 | x | name files in load/save stages, keep bar open through sidecars | V3!V7!V8 |
| T3 | x | report spawn stage progress per file with timings | V3!I.xml |
| T4 | x | single-pass writer for monster+npc spawn XML, byte-identical | V5!V6!I.xml |
| T5 | . | write OTBM to `*.tmp`, fsync, atomic replace; keep `~` as fallback | V1!V2!I.save |
| T6 | . | fsync sidecar XMLs before replace; document crash window | V1!I.xml |
| T7 | . | gzip level setting: measure level 1/3/6 time vs size, pick default | V11!I.io-save |
| T8 | . | log per-stage ms for tiles/houses/zones/waypoints | V8!I.io-save |
| T9 | . | durable result surface: do not lose summary to FPS field | V8 |
| T10 | . | round-trip determinism test in `tests/` (save→load→save, compare) | V5!C7 |
| T11 | . | cancel/fail integration test: kill mid-write, assert restore | V1!V2!V10 |
| T12 | . | backup retention when `ALWAYS_MAKE_BACKUP=true` (N newest) | I.backup |
| T13 | . | measure peak working set for two-DOM spawn writer, set budget | V9 |
| T14 | . | load: decompress progress by bytes, not one step | V3!V7 |

## §B bugs

| id | date | cause | fix |
|---|---|---|---|
| B1 | 2026-10-18 | `layout_manager.cpp` restored palette twice → `wxAuiManager::AddPane` duplicate pane assert | 6f626803!V4 |
| B2 | 2026-10-18 | `wxBusyInfo` centres on screen, closing dialog left editor monitor | 8b8c43e8!V4 |
| B3 | 2026-10-19 | spawn stage reported one percent for both files → 6.8 s frozen UI | T3!T4!V3 |
| B4 | 2026-10-19 | merged writer marked NPC window only for unsaved NPCs → 34 extra empty `<monster>` nodes | T4!V5 |
| B5 | 2026-10-19 | status summary written to field 0, overwritten by FPS tick | T9!V8 |
