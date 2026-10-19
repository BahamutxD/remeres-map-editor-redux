# Goals per phase

The umbrella goal is in [`GOAL.md`](../GOAL.md); this folder splits it into **loop
units**. One phase = one branch = one PR = one goal loop. Do not loop two phases at
the same time: they touch the same files and the runtime keeps a single active goal.

## Order

| phase | goal | tasks | branch | base | gate |
|---|---|---|---|---|---|
| P1 | [`P1-safety.md`](P1-safety.md) | T5, T6, T11, T12 | `fix/save-atomic-replace` | `fix/save-progress-feedback` | V1, V2, V10 |
| P2 | [`P2-speed.md`](P2-speed.md) | T7, T8, T13 | `perf/save-compression-level` | `fix/save-progress-feedback` | M1, M2, M9 |
| P3 | [`P3-observability.md`](P3-observability.md) | T9, T14 | `feat/progress-result-surface` | `fix/save-progress-feedback` | M4, M8 |
| P4 | [`P4-tests.md`](P4-tests.md) | T10 (+ T11 in CI) | `test/save-load-roundtrip` | P1 branch | V5, V1 |

P2 and P3 are independent of P1 and of each other; P4 needs P1's temp-file save path.
The `base` column is stacked on `fix/save-progress-feedback` because PR #1059 is still
under review and touches the same files; retarget to `master` once it merges.

## How to loop a phase

1. Paste the `/goal …` line from the phase file into a new session (or say
   "loop P1" in this one after checking it out).
2. The loop reads, in order: phase file → `GOAL.md` → `SPEC.md` → `PLAN.md` →
   `VERIFY.md`.
3. Work only inside the phase tasks. A finding that belongs to another phase goes to
   `SPEC.md` §B (or §T) — do not expand scope.
4. Each task lands with its evidence row in `VERIFY.md`; a phase closes when every
   success criterion in its goal line has a row with numbers.
5. Report back: metric table (before/after), evidence rows, PR link, §T status.

## Rules shared by all phases

- Branch per phase from its `base`; never commit to `master`.
- Never write to the user's real map directory; copy to `%TEMP%\opencode\…`.
- Evidence only: numbers, SHA256, screenshots. "Looks fine" is not evidence.
- Kill `rme.exe` before building (`LNK1104`).
- Harness pitfalls: a modal dialog swallows `WM_COMMAND`; the binary must run from
  `build-vs2022/Release`; status field 0 is overwritten by the FPS tick.
