# Pokémon Ultra Sun (3DS): Performance boost on vs off — AI1 Emu 0.5.13

- **App:** AI1 Emu test build 0.5.13 (commit fe721f3), launched through the AI1 Emu forwarder
- **Emulator:** Dekopon v3.0.0-RC1, unmodified release (SHA-256 `540809c6…6644175`)
- **Console:** owner's Switch; model, Atmosphère version and handheld/docked mode **not recorded**
- **Clocks:**
  - Run A (Dekopon launch #20): normal clocks, no sys-clk profile for the app
  - Run B (Dekopon launch #21): Performance boost on. AI1 Emu's log shows
    "3DS boost: sys-clk profile applied" before Dekopon started; the sys-clk profile was
    CPU 1785 MHz and memory 1600 MHz in all modes. GPU clock unchanged.
- **Emulator settings:** same Dekopon `config.ini` for both runs (Vulkan, hardware shaders,
  async shader compilation off, disk shader cache on, CPU JIT on, 100% CPU clock)
- **Game:** Pokémon Ultra Sun (USA), title ID 00040000001B5000; revision not recorded
- **Scene:** boot, the opening intro, then the overworld from the save, played by the owner
  by hand. Both runs followed the same steps, but timing and route were not identical.
- **Warm-up / repeats:** shader cache already filled by earlier sessions (no shader
  compile pauses logged in either run). **One run each.**
- **Method:** Dekopon's own `LogPerfStats` lines in `/switch/dekopon/log/azahar_log*.txt`,
  one reading per ~10 seconds (average fps, emulation speed, average and maximum frame
  time, and the frame-time split). "CPU (other)" is the split Dekopon labels `other`,
  which here is dominated by the emulated 3DS CPU. Logging overhead is the same in both
  runs and was not measured.

## Results

Opening intro (the game renders it at 60 fps; first two readings of each run):

| Reading | Run A normal | Run B boost |
| --- | --- | --- |
| ~12 s | 17.1 fps, 29% speed, CPU 52.1 ms/frame | 28.1 fps, 47% speed, CPU 31.8 ms/frame |
| ~22 s | 20.0 fps, 33% speed, CPU 43.7 ms/frame | 33.5 fps, 56% speed, CPU 26.0 ms/frame |

Overworld at full speed (30 fps in both runs):

| | Run A normal | Run B boost |
| --- | --- | --- |
| Full-speed readings | 3 | 7 |
| Average frame time | 16.3–16.8 ms | 9.8–13.1 ms |
| CPU (other) per frame | 7.9–8.0 ms | 4.2–5.2 ms |
| Worst single frame in those readings | 17.9–81.7 ms | 13.6–28.9 ms |

Each run had one slower reading in the overworld (A: 83% speed at ~74 s; B: 74% at ~73 s).
What was on screen at that moment was not recorded.

| Check | Observed result | Evidence / method |
| --- | --- | --- |
| Launch and video | Both runs launched and played | Logs, owner report |
| Input / touch | Not checked | |
| Audio | Not checked | |
| Performance | Measured, see tables (one run each) | Dekopon LogPerfStats |
| Normal save and reload | Not checked | |
| Save state and reload | Not checked | |
| Return to library | Not supported (Dekopon returns to Homebrew Menu) | Known limitation |

## Reading the results

- The boost raised the opening intro from about 17–20 fps to about 28–34 fps, and cut the
  emulated-CPU time per frame by roughly 40%. The intro still runs well below its 60 fps
  target: it needs about 16.7 ms per frame and took 26–32 ms boosted, so it remains
  CPU-bound.
- In the overworld both runs held 30 fps; the boost left more spare time per frame
  (CPU about 5 ms instead of 8 ms), which is headroom for heavier moments.
- A boost reading from an earlier session (launch #19, sys-clk overlay override) showed no
  intro gain; the override was most likely set after the intro had started. This report
  uses only runs where the clock state is confirmed by logs.

## Limitations

One run per condition, hand-played, with slightly different timing. Shader cache warm in
both runs, so shader-compile pauses are not covered. Console model, docked/handheld mode,
battery level and temperature were not recorded. Do not generalise this to other 3DS games.
