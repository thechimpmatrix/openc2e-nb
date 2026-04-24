---
name: Bug report
about: Report a reproducible defect in the engine, CAOS parser, build system, or game integration
title: "[BUG] "
labels: bug
assignees: ''
---

## Summary

A clear, one-sentence description of the defect.

## Steps to reproduce

1. ...
2. ...
3. ...

## Expected behaviour

What should have happened.

## Actual behaviour

What happened instead. Include the full stack trace, crash dump, or engine log output if one is available.

## Environment

- **OS:** (such as Windows 11, Ubuntu 24.04, macOS 14)
- **Compiler / toolchain:** (such as MSVC 2022, GCC 13, Clang 17)
- **CMake version:** (output of `cmake --version`)
- **Python version used for embedding (if the `--brain-module` path is involved):**
- **Game data:** (Creatures 3, Docking Station, or other: version / edition if known)
- **Build configuration:** (Debug, Release, RelWithDebInfo)
- **Commit hash or build date:**

## Scope

Does this affect the NORNBRAIN-specific additions (Python brain module, deferred destruction, TCP CAOS injection), or does it affect upstream openc2e behaviour? Brain-model and training questions belong on the [NORNBRAIN repo](https://github.com/thechimpmatrix/nornbrain) rather than here.

## Additional context

Any bootstrap world that reproduces the issue, relevant CAOS script fragments, or crash log attachments.
