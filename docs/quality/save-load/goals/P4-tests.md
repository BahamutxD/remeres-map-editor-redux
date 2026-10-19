# P4 — tests goal

Loop unit: branch `test/save-load-roundtrip` · base P1 branch
(`fix/save-atomic-replace`) — needs the temp-file save path.

## Goal

The determinism and safety guarantees stop depending on manual harness runs: the test
suite proves that saving the same map twice produces identical sidecars and that a
failed save leaves the previous generation intact.

## Tasks

| id | task | acceptance |
|---|---|---|
| T10 | round-trip determinism test in `tests/` | load → save A → load A → save B; sidecar bytes A == B; counts equal |
| T11 | cancel/fail restore test wired into the suite | previous generation intact; no `.tmp`/`~` on success |

Follow the existing C++ test pattern (`tests/test_otbm_gzip_cpp.cpp`); use a small
fixture map, never the user's world.

## Success criteria

1. `tests/` contains a test that fails if the spawn/house/waypoint/zone XML output is
   not byte-identical across two save passes (SHA256 comparison), and passes today.
2. The test also asserts tile/spawn/house/zone counts are preserved by the round trip.
3. A failure-path test proves the canonical file is untouched when the write or the
   replace fails (inject a failing writer or a read-only target).
4. Both run green locally; the command is documented in `VERIFY.md`.
5. No test writes outside its temp directory.

## Constraints / non-goals

- No new test framework; reuse what `tests/` already uses.
- Keep the fixture small (seconds, not minutes).
- Non-goal: fuzzing, performance tests, CI pipeline changes beyond invoking the test.

## Checks

```
cmake --build build-vs2022 --config Release --target rme --parallel 12
```

Test invocation: document the exact command in `VERIFY.md` once the target exists.

## Evidence

| date | check | result |
|---|---|---|
| | round-trip SHA256 equal | |
| | counts preserved | |
| | failure path leaves canonical | |

## Goal line

```
/goal Fase P4 (tests) del pipeline guardado/carga: docs/quality/save-load/goals/P4-tests.md, SPEC.md V1/V5 y T10/T11. Rama test/save-load-roundtrip desde fix/save-atomic-replace. Objetivo: que la determinacion y la seguridad dejen de depender de corridas manuales del harness. --success "test en tests/ que falla si los XML de spawns/houses/waypoints/zones no son byte-identicos entre dos pasadas de guardado (SHA256), y pasa hoy" --success "el round-trip preserva conteos de tiles/spawns/houses/zones" --success "un test de fallo prueba que el canonico queda intacto si la escritura o el reemplazo fallan" --success "ambos corren verdes en local y el comando queda documentado en VERIFY.md" --constraint "sin framework de tests nuevo; reutilizar tests/ existente" --constraint "fixture chico, nunca el mapa real del usuario" --constraint "ningun test escribe fuera de su directorio temporal" --non-goal "fuzzing o tests de performance" --non-goal "cambios de CI mas alla de invocar el test" --check "cmake --build build-vs2022 --config Release --target rme --parallel 12" --max-turns 200 --max-minutes 1440
```
