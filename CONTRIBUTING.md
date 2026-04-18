## DEVELOPERS! DEVELOPERS! DEVELOPERS!

(If you don't get the reference, go watch the Ballmer clip. Then come back.)

Read this before you touch anything. It will save us both time.

## Ground rules

- Code goes in only if it is understood. This is a system built to be understood.
- No cargo culting. No copying code you cannot explain.
- Keep it simple. If it feels clever, it is probably wrong.
- Respect the existing structure. Do not rewrite things just to "improve" them.

## Code style

- Kernel is C. Keep it straightforward.
- Prefer simple control flow over tricks.
- Name things clearly. If a name needs a comment, the name is bad.
- No comments in code. Zero. If something needs explanation, rewrite it.

## Design

- Small, focused changes.
- Fix the problem, not the symptom.
- Do not mix unrelated changes in one patch.
- No hidden coupling.
- No new abstractions unless they are clearly needed.

## Kernel boundary

- Treat userspace as hostile.
- Do not trust pointers.
- All user input must go through existing validation paths. No shortcuts.
- If you touch exec, paging, or scheduler code, expect strict review and include a failure-path test.

## VFS and namespace

- Do not hardcode paths in multiple places.
- Keep behavior centralized.
- If you need a special case, you probably did something wrong. Prove it is necessary.

## Build and tooling

- Keep `tools/` simple and readable.
- No heavy dependencies.
- Generated code must be deterministic.

## Testing

- If behavior changes and there is no test, it will not be merged.
- Keep tests narrow and deterministic.
- Do not rely on debug prints for correctness.

Run before pushing:

```bash
python3 manage.py verify
```

If it does not pass, it does not go in.

## Commits

One logical change per commit. No drive-by fixes in unrelated files. Say what changed and why.

Good:
```
kernel: fix fd_table_dup leak on full table
```

Bad:
```
fix stuff
```

## AI

AI is allowed. Blindly generated code is not. You are responsible for every line you submit.

If you cannot explain it, delete it.

## What not to do

- Do not add features just because you can.
- Do not refactor unrelated code.
- Do not turn this into a framework.

## Last thing

Clean, small, well-understood change: it gets in. A mess: it does not.

That's it.
