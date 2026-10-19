# Save/load quality workstream

Documents that drive the save/load pipeline work: what we are trying to achieve
(goal), what the pipeline does today (analysis), what must always hold (spec),
what to do next (plan), and how every claim is proven (verify).

## Doc map

| file | role |
|---|---|
| [`GOAL.md`](GOAL.md) | outcome, metrics M1–M9, scope, definition of done, phases |
| [`ANALYSIS.md`](ANALYSIS.md) | pipeline map, measured stages, bottleneck ranking, safety/observability gaps, risks |
| [`SPEC.md`](../../../SPEC.md) | §G goal, §C constraints, §I interfaces, §V invariants (V1–V11), §T tasks, §B bugs |
| [`FORMAT.md`](../../../FORMAT.md) | encoding rules for `SPEC.md` |
| [`PLAN.md`](PLAN.md) | per-task implementation detail, acceptance, branch/PR layout |
| [`VERIFY.md`](VERIFY.md) | harness recipes, measurement protocol, evidence log, pitfalls |

## Where we are

Implemented in PR #1059 (`fix/save-progress-feedback`): T1–T4.

| item | before | now |
|---|---|---|
| save total (53 MB / 19M tiles) | 18.7 s | **14.4 s** |
| spawn stage | 6.8 s at a frozen 91% | **3.0 s, 91→96%** |
| spawn XML output | two scans | **one scan, byte-identical (SHA256)** |
| closing | separate tool window, off-centre, draggable | **same dialog family, centred, app-modal** |
| load | generic stages | **names each sidecar file + result line** |

Next: P1 safety (atomic replace, T5/T6), then P2 speed (gzip level, T7), then
P3/P4.

## How the flow runs

1. `GOAL.md` fixes the outcome and the numbers that define success.
2. `SPEC.md` fixes constraints and invariants; `§T` is the only task list.
3. `build` executes §T on a branch cut from `master`; each task carries evidence.
4. `check` audits §V against code; `backprop` turns a bug into §B + a new §V.
5. `VERIFY.md` is updated with the metric that the task claims.

Bugs already recorded: B1–B5 in `SPEC.md` §B (duplicate pane assert, off-centre
closing window, frozen spawn stage, 34 extra empty spawn nodes, result line lost to
the FPS tick).

## Rules

- One branch/PR per phase (`AGENTS.md` rule 3).
- Tests never write to the user's map directory (C8): copy the map to
  `%TEMP%\opencode\…` first.
- A merge is only justified by numbers, hashes or screenshots in `VERIFY.md`.

## Runbook

1. Branch: `git switch -c <phase-branch> origin/master`.
2. Pick the next `§T` row with status `.`; flip it to `~` in `SPEC.md`.
3. Implement inside the files the task names; keep unrelated changes out.
4. Build: `cmake --build build-vs2022 --config Release --target rme --parallel 12`
   (kill `rme.exe` first, or the link fails with `LNK1104`).
5. Verify with the recipes in `VERIFY.md`; add the metric row with its numbers.
6. Commit (Conventional Commits, English), push, open the PR.
7. Mark the task `x`. Run `check` for §V drift; if a bug appears, run `backprop`
   (new `§B` row, and a new `§V` when the invariant would catch it).

