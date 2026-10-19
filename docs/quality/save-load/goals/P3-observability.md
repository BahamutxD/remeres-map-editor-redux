# P3 — observability goal

Loop unit: branch `feat/progress-result-surface` · base `fix/save-progress-feedback`
(stacked on PR #1059; retarget when it merges).

## Goal

What the pipeline reports survives long enough to be read: the result of a save or a
load (file, size, elapsed) stays visible and is logged, and the load decompression
shows real progress instead of one still step.

## Tasks

| id | task | acceptance |
|---|---|---|
| T9 | durable result line (status field not shared with the FPS tick) | visible ≥ 5 s, no mouse interaction needed |
| T14 | decompress progress by bytes (`Compression::decompressGzipFile` callback) | 0→5 % moves during inflate |

## Success criteria

1. After a save/load, the summary line (`Map saved: … 14.4 s`, `Map loaded: … 19,000,000
   tiles in 6.6 s`) is readable for ≥ 5 s without touching the mouse, and the same
   line is in the log.
2. A 53 MB map load shows more than one decompression step (bytes/total), measured by
   sampling the dialog text.
3. No change to the status bar layout beyond the chosen durable surface.
4. Evidence rows in `VERIFY.md` for both.

## Constraints / non-goals

- Reuse `LoadingManager`/`StatusManager`; no new UI framework.
- Do not remove the FPS tick; make them coexist (dedicated field or sticky message).
- Non-goal: ETA/throughput prediction, toasts, notification centre.

## Checks

```
cmake --build build-vs2022 --config Release --target rme --parallel 12
```

Sample the dialog text every 25 ms during a load (decompress) and screenshot the
status strip right after a save, then again after 6 s.

## Evidence

| date | check | result |
|---|---|---|
| | summary visible ≥ 5 s | |
| | decompress steps observed | |

## Goal line

```
/goal Fase P3 (observabilidad) del pipeline guardado/carga: docs/quality/save-load/goals/P3-observability.md, SPEC.md V3/V8 y T9/T14. Rama feat/progress-result-surface desde fix/save-progress-feedback. Objetivo: el resultado de guardar/cargar se puede leer (no lo pisa el tick de FPS) y la descompresion del load reporta avance real. --success "tras un save/load el resumen (archivo, tamano, tiempo) es legible >=5 s sin tocar el mouse y queda en el log" --success "la carga de un mapa de 53 MB muestra mas de un paso durante la descompresion, medido muestreando el texto del dialogo" --success "no se cambia el layout de la barra de estado mas alla de la superficie elegida" --constraint "reutilizar LoadingManager/StatusManager, sin framework nuevo" --constraint "no eliminar el tick de FPS; deben convivir" --constraint "no escribir en el mapa real del usuario" --non-goal "ETA o throughput predictivo" --non-goal "toasts o centro de notificaciones" --check "cmake --build build-vs2022 --config Release --target rme --parallel 12" --max-turns 200 --max-minutes 1440
```
