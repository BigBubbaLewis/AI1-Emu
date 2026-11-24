# Gameplay benchmarks

There are no standardized, measured hardware benchmark results published in this repository yet. Owner gameplay reports, host tests and successful builds are separate evidence.

## Record a test

For every result, record the app version and commit, core revision, Switch model, Atmosphère version, handheld/docked mode, clock settings and relevant emulator settings. Identify the game, region and revision without sharing a ROM or console keys.

Describe a reproducible scene, test duration, warm-up and number of repetitions. Use the same settings and scene when comparing builds. Record the measurement method. A visual impression of smoothness is a subjective observation and must be labeled accordingly.

Report FPS or frame-time statistics only when actually measured, including units and sample count. Record audio glitches, loading time, crashes and any logging or capture overhead. Record launch/return, input, normal-save reload and save-state round-trip checks independently from performance.

Keep handheld/docked, stock/modified clocks and original Switch/Switch Lite results separate. Note DS layout and touch/virtual-touch behavior. Preserve failed runs as well as successful ones. Do not generalize one game's result to an entire console.

## Report template

Copy this block into an issue or a versioned report under `docs/benchmarks/`. Replace the placeholders with observations and mark untested checks explicitly.

```markdown
# Game / system — app version

- App commit and core revision
- Console model, Atmosphère, handheld/docked
- Clocks and emulator settings
- Game region/revision
- Scene and reproduction steps
- Warm-up, run duration and repeat count
- Measurement method and capture overhead

| Check | Observed result | Evidence / method |
| --- | --- | --- |
| Launch and video | Untested | |
| Input / touch | Untested | |
| Audio | Untested | |
| Performance | Unmeasured | |
| Normal save and reload | Untested | |
| Save state and reload | Untested / unsupported | |
| Return to library | Untested | |

## Limitations and failures

Record inconsistent runs, known issues and the scope of this result.
```

Use your own captures. Keep ROMs, firmware, saves containing personal information, keys and unlicensed cover collections out of reports.
