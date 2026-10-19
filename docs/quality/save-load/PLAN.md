# PLAN — save/load pipeline

Execution plan for [`SPEC.md`](../../../SPEC.md) §T. One branch/PR per phase
(`AGENTS.md` rule 3). Every task lands with evidence rows in [`VERIFY.md`](VERIFY.md).

## Status

| phase | tasks | state |
|---|---|---|
| P0 baseline + harness | — | done (protocol in `VERIFY.md`) |
| P1 safety | T5, T6, T11, T12 | todo |
| P2 speed | T7, T8, T13 | todo |
| P3 observability | T9, T14 | todo (T2, T3 done in PR #1059) |
| P4 tests | T10, T11 | todo |

Done in PR #1059: T1 `a2a3588f`, T2 `b0f17060`, T3 `6d2d43d7`, T4 `047ec107`.

## P1 — safety (branch `fix/save-atomic-replace`)

### T5 — temp file + atomic replace

Problem: today the canonical file is renamed to `~` **before** writing, so between the
rename and a successful write there is no canonical map. A crash relies on
`.saving.txt` + the startup prompt.

Change:

1. Write the OTBM to `<target>.tmp` in the **same directory** (same volume, so the
   replace is atomic).
2. Flush + fsync the temp file (POSIX `fsync`, Windows `_commit`).
3. Replace: POSIX `rename`, Windows `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
4. Same per aux file (XML) once written.
5. Keep the `~` rename only as the rollback path when a replace fails; remove the
   "rename away first" step for the normal path.
6. `ALWAYS_MAKE_BACKUP=true`: copy the previous generation to `.<timestamp>.<ext>`
   **before** the replace (copy, not rename, so the canonical file never disappears).

Files: `source/editor/persistence/editor_persistence.cpp` (backup block :102-160,
rollback :187-257, rotation :287-340), new `source/util/atomic_file.{h,cpp}` (or extend
`FileSystem`) with `bool ReplaceFile(const std::string& tmp, const std::string& target)`.

Acceptance:

- kill the process right after the temp write starts → canonical file still the
  previous generation and loads.
- save success → no `.tmp` left; `~` absent unless a restore failed.
- `ALWAYS_MAKE_BACKUP` on → previous generation preserved as timestamped copy.

Verify: `VERIFY.md` §crash, §save-success.

Risk: platform replace semantics (`ReplaceFileW` vs `MoveFileExW`), antivirus locks,
network shares. Mitigation: single helper, error path reports the temp path.

### T6 — fsync aux files

Same treatment for spawns/houses/zones/waypoints XML; document the remaining crash
window (copy for backup + replace is not one atomic step across files).

### T11 — cancel/fail integration test

Harness test: start a save of the reference copy, click Cancel mid-stage, assert:

- dialog acknowledges within 250 ms (`Cancelling...`),
- canonical file parses and matches the previous generation (size/hash),
- no `.tmp` and no `~` remain.

Also a failure path: make the target read-only → save fails → canonical untouched +
error dialog. Automate in `tests/` where possible (C++ with an injected failing writer)
and keep the kill test as a harness script.

### T12 — backup retention

`ALWAYS_MAKE_BACKUP=true` currently leaves every timestamped copy forever. Add
`Config::BACKUP_RETENTION_COUNT` (default 5) and prune older `.<timestamp>.<ext>`
siblings after a successful rotation. Keep the newest regardless of count errors.

## P2 — speed (branch `perf/save-compression-level`)

### T7 — gzip level setting

Measure level 1, 3, 6 on the reference map (time + output size + load time). Pick the
default that meets M1 (save ≤ 8 s) with < 5% size growth. Add
`Config::OTBM_COMPRESSION_LEVEL` (1–9, default = measured winner) wired to
`io/compression/gzip_node_file_write_handle.cpp` (`gzopen(path, "wbN")`).

Expected: 11.7 s tile stage at level 6 → ~3–5 s at level 1–3; save ~6–8 s total.
gzip is level-agnostic on read, so old clients/loaders are unaffected.

Verify: three saves with different levels, table of `time / size / load time`.

### T8 — per-stage ms logs

Log `Writing tiles … in X ms`, `finishing …`, `houses … in X ms`, `zones …`,
`waypoints …` (spawns already logged). One line per stage, `spdlog::info`.

### T13 — peak memory budget

Measure `PeakWorkingSet64` of the save process with the merged two-DOM writer; record
in `VERIFY.md`. Budget: ≤ baseline + 10 %. If exceeded, fall back to a streaming
writer (also improves the spawn stage), tracked as a follow-up.

## P3 — observability (branch `feat/progress-result-surface`)

### T9 — durable result line

Status field 0 is overwritten by the FPS tick (B5). Move result summaries to a field
the tick does not own, or make the tick skip fields with an active sticky message for
≥ 5 s. Acceptance: after a save/load, the result line is visible for ≥ 5 s without
mouse interaction, including file, size and elapsed.

### T14 — decompress progress

`Compression::decompressGzipFile(path)` reports a single step today. Add a byte
callback every 64 KB chunk and map it to 0..5 % (load), so a 53 MB map no longer shows
a still dialog for ~1.5 s.

## P4 — tests (branch `test/save-load-roundtrip`)

### T10 — round-trip determinism

C++ test in `tests/` (pattern: `test_otbm_gzip_cpp.cpp`):

1. load fixture map,
2. save to temp A,
3. load A, save to temp B,
4. assert sidecar XML bytes A == B (V5), tile/spawn/house/zone counts equal,
5. assert OTBM loads (semantic equality, not compressed bytes).

### T11 — see P1 (same test, both entry points).

## Sequence

```
P1 (T5) ─┬─ T6 ── T11
         └─ T12
P2 (T7, T8) independent; T13 after T7
P3 (T9, T14) independent
P4 (T10) after T5 (needs the temp-file save path)
```

## Gates

- Build green (`cmake --build build-vs2022 --config Release --target rme --parallel 12`).
- `VERIFY.md` rows filled for each task's metric.
- `check` reports no §V drift.
- No change to on-disk formats (C1); gzip level only affects compression (C2).
