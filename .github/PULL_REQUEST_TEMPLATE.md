## Summary

What does this pull request do? One to three sentences.

## Motivation

Why is this change needed? Link to the relevant issue if one exists.

## Changes

- List the files changed and what each change does.
- Keep each entry to one line.

## Scope

What is explicitly NOT changed by this PR? Helps the reviewer understand boundaries and avoid scope creep in review comments.

## Compatibility

- [ ] Does not break compatibility with existing Creatures 3 save files.
- [ ] Does not change the NORNBRAIN Python brain interface (or, if it does, the corresponding change in the [nornbrain repo](https://github.com/thechimpmatrix/nornbrain) is linked).
- [ ] Does not regress behaviour on game data formats that currently work (C3, DS, C1, C2).
- [ ] Does not introduce a new CAOS command or remove an existing one without justification.

## Testing

- [ ] Project builds cleanly on the target platform.
- [ ] Engine launches and loads a bootstrap world without crashing.
- [ ] If the change touches the agent lifecycle or CAOS VM, the deferred-destruction pattern is preserved (`Agent::kill()` marks `pending_kill_`, `World::flushPendingDestroys()` runs at end of tick).

## Licence

- [ ] New source files carry the LGPL-2.1 licence header, consistent with upstream openc2e.

## Checklist

- [ ] Australian English spelling used in user-facing text (behaviour, colour, optimise). Existing upstream American spellings preserved where already present.
- [ ] No hardcoded absolute paths.
- [ ] Errors handled explicitly: no silent swallowing.
