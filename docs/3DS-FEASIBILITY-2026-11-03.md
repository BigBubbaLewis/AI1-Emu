# Nintendo 3DS feasibility review — 2026-11-03

Decision: do not advertise or enable 3DS in the stable AI1 build yet. This review
made no emulator, library, save-path, update-channel or package changes. No new
3DS NRO was built, installed, or hardware-tested.

Current upstream sources reviewed:
- https://github.com/PalindromicBreadLoaf/dekopon
- https://github.com/PalindromicBreadLoaf/dekopon/releases
- https://raw.githubusercontent.com/PalindromicBreadLoaf/dekopon/master/README.md
- https://raw.githubusercontent.com/PalindromicBreadLoaf/dekopon/master/CMakeLists.txt
- https://github.com/Raibatsu/raikopon

Dekopon is an Azahar-based native Switch port. Its current README reports mostly
full-speed gameplay but calls its compatibility list outdated. The latest shown
release is 3.0.0-RC1, explicitly a prerelease. Older releases report hbloader
relaunch crashes and game/session stalls; their current resolution must not be
assumed from those historical reports alone. Raikopon's README claims many games
run at stock clocks but also says development is paused and its latest work may
not be stable. These are upstream claims, not AI1 hardware measurements.

Practical integration approach:
1. Validate specific user-priority games on an original Switch at stock clocks,
   native resolution, no frame generation, and full-memory application mode.
2. Keep the existing AI1 main executable and working cores intact. Use a separate
   native companion rather than mix NXVK and AI1's Mesa/SDL graphics stack.
3. Build the pinned companion and dependencies, then test ROM arguments, reliable
   return to AI1, saves/states, layouts, touchscreen and docked virtual touch.
4. Add a supported-console category, ROM directory, matching data and component
   updates only after that integration actually works. A folder alone is not
   emulator support. 2DS games use the same 3DS software; no separate core is needed.

Local source/SDK blockers verified:
- vendor/dekopon-research is an earlier research snapshot, not a built emulator.
- externals/dynarmic/CMakeLists.txt and externals/nxvk/CMakeLists.txt are absent.
- Installed Switch portlibs do not contain a libnvk/NXVK library.
- Upstream requires its Switch dynarmic/oaknut JIT and a separately built graphics
  stack. Its README states NXVK and switch-mesa cannot be linked simultaneously.
- The native source accepts a ROM in argv[1], which is promising for chainloading;
  this does not establish that returning to AI1 or repeated launches are reliable.
- Any candidate build must audit crypto/system-data assets, disable upstream's
  default ENABLE_BUILTIN_KEYBLOB option where present, and bundle no proprietary
  keys, firmware, commercial games, or proprietary frame-generation DLL.

Do not infer stock-speed compatibility from increased-resolution or frame-
generation demonstrations. The developer specifically describes significant GPU
cost/overclocking for the optional frame-generation feature; it is not needed for
ordinary 3DS emulation and should remain disabled for this project.

Next evidence needed: exact target game names and a standalone, stock-clock
hardware trial. Compilation alone cannot satisfy the user's 'runs well' condition.

## Requested game targets

The owner prioritizes Pokemon Ultra Sun, Ultra Moon, X, Y, Omega Ruby and Alpha
Sapphire, with Zelda and Mario titles secondary. These are trial targets, not a
confirmed AI1 compatibility list.

Primary title-specific evidence reviewed:
https://github.com/Raibatsu/raikopon/releases/
The fork's historical notes describe Pokemon X/Y battle slowdown associated with
fastmem, and an experimental movie-throttle workaround used for Ultra Sun/Moon.
These establish game-specific tuning concerns, not current stock-clock pass/fail
results. No reliable stock-clock measurements for all six requested titles were
verified in this review. Do not describe Omega Ruby/Alpha Sapphire as confirmed
full-speed based on PC Azahar compatibility or footage from another Switch setup.

Minimum trial for each title: start a new game, reach normal exploration and a
battle, test a demanding scene/transition, listen for audio slowdown, create a
normal save, close/reopen and reload it, and test sustained play plus exit/relaunch.
Use 1x/native resolution, stock clocks and frame generation disabled. Record game
region/update, emulator version, performance settings and shader-cache warm/cold
status. Reaching the title screen alone is not a compatibility pass.

No 3DS integration or stable-channel update was published from this assessment.
