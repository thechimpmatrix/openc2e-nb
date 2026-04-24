# openc2e Session Report: Clean C3 Bootstrap on 64-bit Windows 11

**Date:** 2026-03-31
**Branch:** `openc2e-nornbrain`
**Platform:** Windows 11 Pro N (10.0.26200), AMD 9950X3D, x86-64
**Game data:** GOG Creatures Exodus / Creatures 3

## Result

openc2e boots GOG Creatures 3 cleanly on 64-bit Windows 11 with zero engine errors. All bootstrap scripts load successfully. The world runs with ~1000 agents at 256-556 FPS.

## Session Metrics

| Metric | Value |
|--------|-------|
| Duration | 5.0 minutes |
| Log entries | 14,842 |
| Entry rate | 49.5/sec |
| Errors (engine) | 0 |
| Errors (game data) | 2 (bacteria.cos null agent) |
| Warnings (engine) | 0 |
| Warnings (game data) | 3 (bacteria + missing norn sprite) |
| Perf samples | 59 |

## Performance

| Metric | Min | Max | Avg |
|--------|-----|-----|-----|
| Tick | 2ms | 13ms | 3.8ms |
| Frame | 46ms | 61ms | 52ms |
| Agents | 930 | 1,067 | 1,002 |
| FPS | 256 | 556 | 385 |

## Severity Breakdown

| Level | Count | % |
|-------|-------|---|
| DBG | 14,276 | 96.2% |
| INF | 502 | 3.4% |
| PERF | 59 | 0.4% |
| WRN | 3 | 0.0% |
| ERR | 2 | 0.0% |

## Issues Found (all game data, not engine)

1. **bacteria.cos null agent** (2x ERR + 2x WRN) -- Bootstrap script `bacteria.cos` tries to target an agent that no longer exists. This is a bug in the original game data, not the engine.

2. **Missing norn sprite `n60a`** (1x WRN) -- A norn body part sprite not found. Likely a breed-specific sprite not shipped with the base C3 install.

## What Was Fixed (this session)

### CAOS Parser Error Tolerance
- `meerk_fix.cos` has `scrp 2 15 23 9"` (trailing quote). The lexer correctly produces a `TOK_ERROR` token, but the parser was keeping it in the token stream and crashing.
- **Fix:** Filter `TOK_ERROR` tokens out of the token stream during parsing. Log a warning to stderr when skipping.
- **Result:** meerk_fix.cos loads cleanly. All 22 lexer tests pass.

### Missing Sprite Handling
- Five DS-only `.c16` files (hand cursor, font sprites) are missing from C3-only installs.
- **Fix:** Log once per unique missing file at INF level instead of WRN per occurrence.
- **Result:** Clean log output, each missing file reported exactly once.

### File Resolution Warnings
- `PathResolver::findFile()` logged WRN for every failed resolution. The engine probes multiple directories by design.
- **Fix:** Downgrade to DBG.

### Missing Script Warnings
- `Agent::fireScript()` logged WRN when an agent doesn't define an optional script. The engine probes for scripts 0-5, 9, 12-14, 92 on all agents.
- **Fix:** Downgrade to DBG.

## Before/After Comparison

| Metric | Before fixes | After fixes |
|--------|-------------|-------------|
| ERR (engine) | 2 (meerk_fix.cos) | 0 |
| WRN (engine) | 17 | 0 |
| ERR (game data) | 0 | 2 (bacteria.cos) |
| WRN (game data) | 0 | 3 (bacteria + sprite) |
| **Total issues** | **19** | **5** (all game data) |

## Commit History

```
88daf5b8 fix: CAOS parser skips lexer error tokens (meerk_fix.cos trailing quote)
baadae66 fix: clean C3 bootstrap: stub sprites, downgrade warnings, skip missing scripts
```

## How to Reproduce

```bash
# Build
cd <PROJECT_ROOT>/openc2e/build64
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . --config RelWithDebInfo --parallel 16

# Run
cd RelWithDebInfo
./openc2e.exe --data-path "<PROJECT_ROOT>/creaturesexodusgame/Creatures Exodus/Creatures 3" --gamename "Creatures 3"

# Monitor (separate terminal)
python <PROJECT_ROOT>/openc2e/tools/nornwatch.py --port 9999

# Logs appear in: build64/RelWithDebInfo/logs/openc2e-*.jsonl
```
