# Physical Switch validation still required

Test the approved frozen candidate at stock clocks on an original Switch and a homebrew-enabled Switch Lite where available. Record app/core hashes, Atmosphère version, launch method, handheld/docked mode and ROM region/revision. A missing second device remains explicitly untested.

## First launch and library

1. Back up the existing working app and all saves. Extract the full candidate into a clean SD layout and launch in full-memory Homebrew Menu application mode.
2. Confirm all console folders, companions, fonts and databases are found. Test an empty library, then a small and large library. Confirm no extra nested package folder.
3. Check Home/Library/Settings at 1280×720 and docked output. Test D-pad, both sticks, A/B, all tab/filter/search/sort controls and keyboard cancel.
4. Drag/flick the touchscreen carousel or shelf, tap select/launch and test touch after docking/undocking. Record pauses, accidental launches and focus problems.
5. Test default white/black themes, each font, accent changes, custom theme save/reload and restart. Check long titles, accented filenames, missing and invalid covers, and large images.
6. Confirm duplicate suppression does not delete ROMs or saves. Existing-save selection and same-title regional/revision variants must remain correct.

## Each core and supported system

Use at least one legally supplied game per GB, GBC, GBA, NES, SNES, Genesis, Master System, Game Gear, DS, DSi if available and N64. Record exact titles rather than claiming a whole console passes from one game.

- Launch to real video and audio, play at least 15 minutes, test all relevant buttons and return to the library. Launch another game/core afterward.
- Save normally in game, exit cleanly, relaunch and verify progress. Repeat after a full app close. Test console/game separation for identical basenames.
- Save/load quick and manual slots where supported; verify state thumbnails, overwrite/delete/cancel and corrupt/incompatible-state errors. Test autosave on controlled exit and resume. Preserve DSi's unsupported-state behavior.
- Open/close the new quick menu with controller and touch. Check that emulation/audio pause correctly and resume without stuck input.
- For a supported per-game cheat, verify an actual visible effect, disable it and confirm persistence. Reject malformed/oversized files gracefully. Do not treat the preview's sample code as valid for every game.
- Test rewind and fast-forward per core. Record memory, responsiveness and audio behavior. Disable or limit features that exhaust memory or create repeated stalls.
- Test display scaling, filters, volume and per-game overrides without damaging ordinary saves.

## DS / DSi / N64 focus

DS needs all three screen layouts, correct bottom-screen hit mapping, handheld touch, docked virtual cursor, prolonged loud music and state-load audio recovery. The owner previously reported choppy music in Pokémon HeartGold and Zelda Phantom Hourglass without gameplay slowdown. Reproduce that observation and record actual audio timing, using the user's own backups.

DSi requires the user's own firmware/NAND and verified direct load paths. Keep untested devices/games and unsupported states explicit. N64 requires per-game GPU/input/audio checks, Minus+ZR / Minus+ZL shortcuts where retained, memory/JIT-path logs and safe return to the library. Do not infer full-speed performance from compilation.

## Reliability and updates

Use expendable test fixtures or a spare SD card for failure-path tests. Test low free space, failed writes, missing core, missing firmware, malformed cover/cheat/state files, interrupted/cancelled downloads and failed staged installation. Confirm rollback and previous saves survive. Do not deliberately interrupt a write on the user's only save copy.

Confirm stable versus preview update routing is preserved, TLS works on actual Switch hardware and the new UI's check/download/install controls invoke the correct updater path. Reopen after install, compare hashes and confirm games/saves/themes/cheats/custom art persist. No production deployment or channel change is authorized by this preparation.

## Performance report

For each tested game, define the scene and a warm-up, then record at least three equal-duration runs where practical. Record the actual measurement method and any capture/logging overhead. Supply FPS/frame-time, audio underruns, peak memory and loading time only if measured. Otherwise label observations subjective. Leave unmeasured fields blank. No overclocking is assumed or required.

Public screenshots should come from the final candidate and use authorized imagery. The staged PNGs show desktop UI fixtures only.
