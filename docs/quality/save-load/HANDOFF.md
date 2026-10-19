# HANDOFF — save/load pipeline quality

Brief to hand to the next session/agent. Everything below is already in the repo:
read this file, then `GOAL.md`, then `SPEC.md`, then `PLAN.md` before touching code.

## 1. Activate the goal

A goal record already exists at `.opencode/goals/dc74e3f2cd9c04c7a5ac9992648294b7.json`
(workspace-local, not committed: it carries the session id). If the runtime does not
show it as active, create it through the app with this command (single line):

```
/goal Elevar la calidad del pipeline de guardado/carga del editor (docs/quality/save-load/GOAL.md y SPEC.md): confiable, observable y rapido. Empezar por T5 (temp + reemplazo atomico + fsync) y seguir T6/T11/T12, luego T7/T8/T13, despues T9/T14 y T10. Una rama/PR por fase desde master; evidencia numerica/hash/screenshot en VERIFY.md; nunca escribir en el mapa real del usuario. --success "guardar el mapa de referencia (53.252.872 B, 19.0M tiles, 53.788 areas de spawn) en <=8 s y cargarlo en <=7 s, medido con el protocolo de VERIFY.md y publicado ahi" --success "matar el proceso a mitad de escritura deja el mapa anterior cargable y sin *.tmp; cancel deja el mapa anterior intacto y sin '~' en exito" --success "guardar el mismo mapa dos veces produce sidecars XML byte-identicos (SHA256), igual que el round-trip save->load->save" --success "cada etapa nombra archivo/fase con gap <=1 s, editor no movible/usable durante la operacion, resultado (archivo+tamano+tiempo) visible >=5 s y en el log" --success "pico de memoria del save medido y dentro del presupuesto (V9)" --constraint "no cambiar formatos on-disk (OTBM/XML); el nivel gzip si puede cambiar" --constraint "cancel sigue disponible durante el save" --constraint "reutilizar LoadingManager, sin framework nuevo" --constraint "tests nunca escriben en el directorio del mapa del usuario" --non-goal "paralelizar el escaneo de spawns" --non-goal "gzip en bloques o streaming writer" --non-goal "motor de replace/add/delete" --check "cmake --build build-vs2022 --config Release --target rme --parallel 12" --max-turns 200 --max-minutes 1440
```

## 2. What is already done (do not redo)

PR #1059 (`fix/save-progress-feedback`, HEAD `5d4561f9`): T1–T4 — unified app-modal
dialogs for closing/loading/saving, file names in every load/save stage, spawn
progress per file, single-pass spawn writer with byte-identical output, result lines
in log + status.

Measured on the reference map: save 18.7 s → **14.4 s**; spawn stage 6.8 s frozen →
**3.0 s** moving; load 6.6–7.1 s.

## 3. Start here

**T5 — temp file + atomic replace** (`PLAN.md` P1). Today the canonical map is renamed
to `~` *before* writing, so a crash leaves no canonical file and recovery depends on
`.saving.txt`. Change `EditorPersistence::saveMap` and the writers to:

1. write `<target>.tmp` in the target directory;
2. flush + fsync (`_commit` on Windows, `fsync` on POSIX);
3. replace atomically (`MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`; POSIX `rename`);
4. same for each aux XML;
5. keep `~` only as the rollback path when a replace fails.

Acceptance: kill the process mid-write → canonical file is the previous generation and
loads; success → no `.tmp`, no `~`; with `ALWAYS_MAKE_BACKUP` on the previous
generation survives as `.<timestamp>.<ext>`.

Then T6 (fsync aux), T11 (cancel/kill test), T12 (backup retention).

## 4. Rules (non-negotiable)

- One branch/PR per phase cut from `master` (`AGENTS.md` rule 3).
- Never write to the user's real map dir; copy to `%TEMP%\opencode\…`.
- Evidence or it did not happen: numbers, SHA256, screenshots, in `VERIFY.md`.
  "Looks fine" is not evidence; this work already produced a 34-node XML delta that
  only a byte comparison caught (`B4`).
- Kill `rme.exe` before building (otherwise `LNK1104`).
- Keep the harness pitfalls in mind: a modal dialog swallows `WM_COMMAND`; the binary
  must run from `build-vs2022/Release`; status field 0 is overwritten by the FPS tick.

## 5. Definition of done (goal level)

All §V invariants V1–V11 verified with evidence; M1–M9 at target or documented
exception; round-trip + cancel tests in `tests/`; `check` reports no drift; build green.

Report back with: metric table (before/after), the evidence rows added, and the PR link.
