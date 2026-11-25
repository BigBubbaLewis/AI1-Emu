# Known issues

Applies to **test build 0.5.13** (private test channel), last updated 2026-11-25.
The public downloads are still stable 0.3.5 and UI preview 0.4.0; their issues are in
their own release notes.

Evidence is labelled: **measured** means read from emulator logs on the owner's
Switch (single sessions, uncontrolled scenes, not benchmarks under
[BENCHMARKING.md](BENCHMARKING.md)); **reported** means the owner observed it; **untested**
means nobody has checked it on a Switch yet.

## All consoles

- **Full-memory mode is required for DS, DSi, N64 and 3DS.** Open AI1 Emu as an
  application (a forwarder, or hold R while starting a game), not from the album.
- **Covers are matched by file name.** Missing covers are downloaded from
  libretro-thumbnails. A game whose file name does not match the No-Intro name gets no
  cover, and AI1 Emu writes `<name>.none` in `artwork-cache/<console>/` so it does not
  ask again. Delete that file to retry, or put your own picture in `covers/<console>/`.
- **Automatic update checks** run when the app opens and every 5 hours while it is open.
  DS, DSi, N64, NES, SNES, Genesis and 3DS run as separate programs, so no check happens
  while those games are running; one runs on return if 5 hours have passed. A found
  update is announced and nothing installs without pressing the button.
- **Duplicate detection** compares games over 64 MB at 32 sample points instead of
  byte by byte (full comparison of multi-GB 3DS copies froze startup in 0.5.5–0.5.10).
  Two different large files with identical size and identical samples would be treated
  as copies; no such case is known.
- **Performance boost** (Settings > Gameplay) raises the CPU to 1785 MHz and, for 3DS,
  writes a sys-clk profile with CPU 1785 MHz and memory 1600 MHz. These are within
  Nintendo's own limits but use more battery and run warmer. Its effect is **not yet
  measured** in a controlled before/after test.
- **Screenshots in the README** are v0.5.0 captures from the Windows preview. Later
  layout changes (console filter row, downloaded covers) are not shown.

## Nintendo 3DS (Dekopon v3.0.0-RC1, unmodified release)

- **Quitting a 3DS game returns to the Homebrew Menu**, not to AI1 Emu.
- **AI1 Emu's in-game menu does not apply.** Save states, cheats, layouts and options
  come from Dekopon's own menu (+ and −). Play time is not counted; the launch is
  recorded so the game appears under Recently played.
- **Shader stutter the first time something appears** (new area, battle start, a move's
  effect). Measured in Pokémon Ultra Sun before the shader cache was filled: 11 pauses
  of 0.1–1.2 s in about 70 s. Pauses become rarer as Dekopon's shader cache fills.
- **Do not turn on "Async Shader Compilation"** (Dekopon Settings > Graphics > Shaders).
  It removes the stutter, but in Pokémon Ultra Sun some Pokémon models were drawn red,
  and the broken shaders stayed in the cache until it was cleared (Settings > Graphics >
  Clear Shader Cache). Reported and confirmed by clearing the cache. AI1 Emu 0.5.8–0.5.11
  turned this setting on automatically; 0.5.12 and later never change Dekopon's settings.
- **CPU-bound slowdowns** in demanding scenes. Measured: Pokémon Y stayed at about 10 fps
  (33–38% speed) for about two minutes; Pokémon Ultra Sun's opening cutscene ran at
  16–34 fps (27–57% speed) in a session where the owner had set a sys-clk boost.
- **One 34-second graphics stall** was logged once in Pokémon Ultra Sun. Cause unknown,
  not reproduced.
- **Local wireless is not implemented** in Dekopon (`nwm::UDS` calls are logged as
  unimplemented). Features that use it may not work.
- Some games need system files from your own 3DS (for example shared fonts). None are
  included.

## Nintendo DS / DSi (melonDS)

- **Sound is stretched** (slightly lower pitch) while a game runs below full speed, so it
  stays continuous instead of cutting out.
- **No rewind**; fast forward is in the menu only (the sticks move the touch cursor).
- **One crash during play** was logged on 2026-11-24 with a build that predated the
  0.5.7 build fix. It has not been reproduced since; the cause is not confirmed.
- **DSi is experimental**: your own BIOS, firmware and NAND are required, and save
  states are unsupported.

## Nintendo 64 (Mupen64Plus-Next)

- **Experimental.** Game performance on a Switch is untested. No rewind (states are too
  large). The in-game menu has no on-screen messages over the running game.

## Not yet checked on a Switch

Save-state round trips on every console, N64 gameplay, the 5-hour update check during a
long session, and the 3DS performance boost through the AI1 Emu setting (rather than the
sys-clk overlay) are untested.
