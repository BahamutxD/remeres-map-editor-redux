# GOAL — save/load pipeline quality

Status: active · Spec: [`SPEC.md`](../../../SPEC.md) · Plan: [`PLAN.md`](PLAN.md) ·
Evidence: [`VERIFY.md`](VERIFY.md) · Baseline: [`ANALYSIS.md`](ANALYSIS.md) ·
Handoff: [`HANDOFF.md`](HANDOFF.md)

## Goal

Make the editor's save/load pipeline **trustworthy, observable and fast**:

1. **Trustworthy** — a save either fully replaces the map or leaves the previous
   generation intact; cancel and crash never produce a half-written canonical file.
2. **Observable** — every stage names the file it is working on and moves; each
   operation ends with one line stating file, size and elapsed time.
3. **Fast** — the reference map (53 MB, 19.0M tiles, 53,788 spawn areas) saves in
   ≤ 8 s and loads in ≤ 7 s on the reference machine, with no UI freeze > 1 s.

## Why now

Measured on the reference map before this workstream (see `ANALYSIS.md`):

| symptom | baseline | after the current PR |
|---|---|---|
| save total | 18.7 s | **14.4 s** |
| spawn stage | 6.8 s frozen at 91% | **3.0 s, 91→96% moving** |
| closing modal | separate window, off-centre, draggable | **same dialog family, centred, not draggable** |
| sidecar determinism | unknown | **byte-identical (SHA256) after single pass** |

The remaining 11.7 s of the save is tile writing at gzip level 6 — untouched so far.

## Success metrics

| # | metric | baseline | target | measured by |
|---|---|---|---|---|
| M1 | save total, reference map | 14.4 s | ≤ 8 s | `VERIFY.md` §timing |
| M2 | spawn stage | 3.0 s | ≤ 3 s | stage timestamps |
| M3 | load total | 6.6–7.1 s | ≤ 7 s | log line `Map loaded` |
| M4 | longest frozen interval in UI | 6.8 s (fixed) | ≤ 1 s | dialog text sampling |
| M5 | cancel acknowledgement | unmeasured | ≤ 250 ms | harness cancel click |
| M6 | partial canonical file after crash | possible (rename-away) | never | kill mid-write test |
| M7 | aux XML byte stability | spawns: yes | all aux files | SHA256 before/after |
| M8 | result line coverage | status only, lost to FPS | 100% saves/loads | log + status |
| M9 | peak save working set | unmeasured | ≤ baseline + 10% | `VERIFY.md` §memory |

M6 and M9 are the two not yet measured; they drive the safety phase.

## Scope

In: OTBM save/load, auxiliary XML writers/readers, progress dialogs, cancel,
backups (`~` + `.saving.txt`), crash recovery prompt, stage logging, status output.

Out: map format changes, new OTBM versions, replace-tool engine, live/collab,
importer, minimap, renderer.

## Non-goals

- No speculative parallelization of the spawn scan (needs global claim/dedupe; separate
  design, see `ANALYSIS.md` §follow-ups).
- No new progress framework; extend `LoadingManager`.
- No silent behaviour change for `ALWAYS_MAKE_BACKUP` users; retention is additive.

## Definition of done

1. §V invariants V1–V11 verified with evidence rows in `VERIFY.md`.
2. Metrics M1–M9 at target on the reference map, or an explicit, documented exception.
3. `tests/` has a round-trip determinism test and a cancel/fail restore test.
4. `check` (spec vs code) reports no drift for §V.
5. Build green on the CI matrix; no new warnings.

## Phases

| phase | theme | tasks | gate |
|---|---|---|---|
| P0 | baseline + harness | — (done) | measurement protocol reproducible |
| P1 | safety | T5, T6, T11, T12 | M6, V1, V2, V10 |
| P2 | speed | T7, T8, T13 | M1, M2, M9 |
| P3 | observability | T9, T14 (+T2/T3 done) | M4, M8, V3, V7, V8 |
| P4 | tests | T10, T11 | determinism + restore tests in CI |
| P5 | follow-ups | — (separate specs) | parallel spawn scan, parallel inflate |

Each phase is a branch/PR cut from `master`, per `AGENTS.md` rule 3.

## Workflow

1. `GOAL.md` (this) fixes the outcome and metrics.
2. `SPEC.md` fixes constraints, invariants and the task table (§T).
3. `build` executes §T tasks; every task lands with its verification evidence.
4. `check` audits drift (§V vs code); `backprop` turns any bug into §B + a new §V.
5. `VERIFY.md` is the only accepted proof: numbers, hashes, screenshots. "Looks fine"
   is not evidence.
