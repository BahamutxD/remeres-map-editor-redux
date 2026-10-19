# P1 — safety goal

Loop unit: branch `fix/save-atomic-replace` · base `fix/save-progress-feedback`
(stacked on PR #1059; retarget to `master` when it merges).

## Goal

The saved map is never partial or missing while a save runs: the OTBM is written to a
temporary file in the target directory, flushed to disk, and atomically replaced. A
kill or a cancel leaves the previous generation loadable.

## Tasks

| id | task | acceptance |
|---|---|---|
| T5 | OTBM to `*.tmp` + `fsync`/`_commit` + atomic replace; drop the rename-away of the map file | kill mid-write → previous map loads; success → no `.tmp` |
| T6 | sidecar XML to temp + sync + replace; document the cross-file window | same per file; window documented in code |
| T11 | cancel/kill integration test | cancel ≤250 ms, previous map intact, no `~`/`.tmp` on success |
| T12 | backup retention for `ALWAYS_MAKE_BACKUP` | newest N kept, older pruned |

T5 lands first and keeps the current `~` rollback for sidecars; T6 unifies them so the
cancel path never has to restore a renamed-away file.

## Success criteria

1. `world.otbm` written as `world.otbm.tmp`, synced, then replaced atomically
   (`MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` on Windows, `rename` on POSIX).
2. `Stop-Process -Force` mid-write → canonical map is the previous generation, loads,
   same SHA256 as before the attempt; no orphan `.tmp`.
3. `ALWAYS_MAKE_BACKUP` keeps the previous generation as `.<timestamp>.otbm` (copy
   before the replace, not rename).
4. No `~` and no `.tmp` remain after a successful save.
5. Evidence rows in `VERIFY.md` for kill, cancel and success paths.

## Constraints / non-goals

- No on-disk format change; only how files are written.
- Keep the existing sidecar rollback until T6 lands.
- Never write to the user's map directory in tests.
- Non-goal: cross-file transactional commit (documented window, T6), parallel gzip.

## Checks

```
cmake --build build-vs2022 --config Release --target rme --parallel 12
```

Harness: `VERIFY.md` §4 (byte comparison), §8 (crash / cancel).

## Evidence

| date | check | result |
|---|---|---|
| | kill mid-write | |
| | success: no `.tmp`/`~` | |
| | cancel ≤250 ms | |
| | backup copy kept | |

## Goal line (paste into a new session)

```
/goal Fase P1 (seguridad) del pipeline guardado/carga del editor: docs/quality/save-load/goals/P1-safety.md, SPEC.md V1/V2/V10 y T5/T6/T11/T12. Rama fix/save-atomic-replace desde fix/save-progress-feedback (apilada hasta que #1059 se mergee). Objetivo: el mapa guardado nunca queda parcial ni ausente durante el save; cancel y kill preservan la generacion anterior. --success "el .otbm se escribe a *.tmp en el mismo directorio, se sincroniza (fsync/_commit) y se reemplaza atomicamente (MoveFileExW REPLACE_EXISTING|WRITE_THROUGH en Windows, rename en POSIX); el canonico nunca queda parcial" --success "matar el proceso a mitad de escritura deja el mapa anterior cargable, sin *.tmp huerfano y con el mismo SHA256 que antes del intento" --success "cancelar deja el mapa anterior intacto; en exito no quedan '~' ni '*.tmp'; si el reemplazo falla se reporta el path y el canonico sigue siendo el anterior" --success "ALWAYS_MAKE_BACKUP conserva la generacion anterior como copia timestamped antes del reemplazo, con retencion configurable" --constraint "no cambiar formatos on-disk (OTBM/XML), solo el modo de escritura" --constraint "mantener el rollback actual de los XML auxiliares hasta T6" --constraint "no escribir en el mapa real del usuario; copias en %TEMP%" --non-goal "commit transaccional entre archivos" --non-goal "gzip en bloques o streaming writer" --check "cmake --build build-vs2022 --config Release --target rme --parallel 12" --max-turns 200 --max-minutes 1440
```
