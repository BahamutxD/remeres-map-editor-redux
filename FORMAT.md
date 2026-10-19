# FORMAT — spec encoding

Rules for `SPEC.md`. Compressed ("caveman") prose: drop articles and filler, keep
imperatives, preserve identifiers/paths/code verbatim. One fact per row.

## Sections (fixed order)

| id | name | content |
|---|---|---|
| §G | goal | one line, measurable outcome |
| §C | constraints | hard limits that cannot change |
| §I | interfaces | public surfaces touched (file:line where useful) |
| §V | invariants | numbered, testable statements (V1…) |
| §T | tasks | pipe table, one row per unit of work |
| §B | bugs | pipe table, one row per post-spec bug |

## §V invariants

- Number monotonic. Never reuse a number.
- Each invariant must be falsifiable: name what is observed and the expected result.
- Keep to one sentence; put measurement detail in `docs/quality/save-load/VERIFY.md`.

## §T tasks

Header: `id|status|task|cites`

- `id`: `T1`, `T2`… monotonic.
- `status`: `.` todo, `~` doing, `x` done, `!` blocked.
- `cites`: `!`-separated §V/§I deps, e.g. `V1!I.save`. Empty allowed.

## §B bugs

Header: `id|date|cause|fix`

- `id`: `B1`, `B2`… monotonic.
- `date`: `YYYY-MM-DD`.
- `cause`: root cause, one line, name file/symbol.
- `fix`: task id or commit, plus the invariant that catches recurrence (`V5`).
- Every bug found after the spec gets a row, even when the fix is trivial.

## Writing

- No new section names. No prose blocks inside §T/§B.
- Uncertain facts end with `?` so the reader can confirm them.
- Prefer numbers over adjectives: `2.9 s`, `10,103,410 B`, `53,788 spawn areas`.
