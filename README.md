<p align="center"><img src="docs/images/banner.svg" alt="AI1 Emu — All In 1 Emulator" width="100%"></p>

<p align="center"><strong>Your retro library. One place to play.</strong><br>A controller-and-touch homebrew frontend for Nintendo Switch.</p>

<p align="center"><a href="https://ai1emu-updates.freegamez.workers.dev/">Downloads</a> · <a href="#install">Install</a> · <a href="#systems">Systems</a> · <a href="CONTRIBUTING.md">Contribute</a></p>

<p align="center"><img alt="Stable channel 0.3.5" src="https://img.shields.io/badge/stable-0.3.5-2018ca"> <img alt="UI preview 0.4.0" src="https://img.shields.io/badge/UI_preview-0.4.0-e3fa3d"> <img alt="Switch homebrew" src="https://img.shields.io/badge/platform-Switch_homebrew-ed35a2"></p>

AI1 Emu brings a Home screen that picks up where you left off, a searchable library with favourites, themes you can make your own, and an in-game menu with save slots, rewind, fast forward and cheats. Gameplay works offline. Optional Windows artwork tools and in-app updates help keep your library ready to play.

## See the interface

<img src="docs/images/home.png" alt="Actual v0.5.0 Home screen from the Windows UI preview" width="100%">

<details><summary>Library, in-game menu, themes and controls</summary>

<img src="docs/images/library.png" alt="Actual v0.5.0 Library screen from the Windows UI preview" width="100%">
<img src="docs/images/game-menu.png" alt="Actual v0.5.0 in-game menu, Save state page, from the Windows UI preview" width="100%">
<img src="docs/images/game-menu-speed.png" alt="Actual v0.5.0 in-game menu, Speed page, from the Windows UI preview" width="100%">
<img src="docs/images/settings.png" alt="Actual v0.5.0 Themes settings from the Windows UI preview" width="100%">
<img src="docs/images/home-black.png" alt="Actual v0.5.0 Home screen in the Black theme from the Windows UI preview" width="100%">
<img src="docs/images/controls.png" alt="Actual v0.5.0 Controls screen from the Windows UI preview" width="100%">

</details>

These v0.5.0 captures come from the Windows preview using the app's shared SDL2 renderer, with free homebrew titles and no cover art. The preview does not emulate games: the in-game picture is generated. v0.5.0 is built and host-tested but not yet released or tested on a Switch; see [v0.5.0 notes](docs/V0.5.0.md).

## Download

| Channel | Download | Status |
| --- | --- | --- |
| Stable 0.3.5 | [Complete Switch package](https://ai1emu-updates.freegamez.workers.dev/releases/0.3.5/AI1-Emu-v0.3.5-Switch-Package.zip) | Existing multi-console release |
| UI preview 0.4.0 | [Complete Switch package](https://ai1emu-updates.freegamez.workers.dev/releases/0.4.0/AI1-Emu-v0.4.0-Switch-Package.zip) | Five-tab interface, compiled and host-tested, physical Switch validation pending |
| Source | [Published v0.4.0 source archive](https://ai1emu-updates.freegamez.workers.dev/source/0.4.0/AI1-Emu-v0.4.0-Corresponding-Source.tar.gz) | App, cores, notices, artwork tools and update service |

Use the complete package for a fresh installation. The standard in-app update channel remains on 0.3.5. Downloads are currently hosted on Cloudflare.

The v0.5 builds described below (latest: test build 0.5.13) are on a private test channel and are not a public release yet. See [known issues](docs/KNOWN-ISSUES.md).

## Install

Requires an original Switch or homebrew-enabled Switch Lite running Atmosphère, with Homebrew Menu available. Use full-memory application mode for the multi-console cores.

1. Download a complete package and extract it on your computer.
2. Merge its `switch` folder into the SD root. Preserve existing ROMs, saves and custom artwork.
3. Put your legally obtained, uncompressed game backups in the matching console folder under `switch/ai1emu/roms/`.
4. Launch AI1 Emu through Homebrew Menu in application mode.

```text
SD:/switch/ai1emu/
├── AI1Emu.nro
├── cores/
├── roms/
│   ├── gba/   gb/   gbc/   nes/   snes/
│   ├── genesis/   sms/   gg/
│   └── nds/   dsi/   n64/   3ds/
├── covers/
├── artwork-cache/
├── saves/
└── system/
```

The app creates console folders at startup. The package's `PC-tools` folder is for Windows. Run the self-contained DBI cover CMD with DBI's MTP responder open to prepare artwork. Cover availability varies by game and filename. Custom images go under `covers/<console>/` with the same basename as the ROM, including accents and version numbers.

## Systems

These systems have emulator integrations. Compatibility and speed depend on the game and core.

| System | Core | ROM extensions | Status / requirements |
| --- | --- | --- | --- |
| Game Boy Advance | mGBA | `.gba` | Owner-reported gameplay on an original Switch |
| Game Boy / Game Boy Color | mGBA | `.gb`, `.gbc` | Color-capable GB headers detected |
| NES | FCEUmm | `.nes` | Standard cartridge games |
| SNES | Snes9x | `.sfc`, `.smc` | Some enhancement-chip games need user-supplied data |
| Genesis / Mega Drive | Genesis Plus GX | `.md`, `.gen`, `.smd`, supported `.bin` | Cartridge systems |
| Master System / Game Gear | Genesis Plus GX | `.sms`, `.gg` | Cartridge systems |
| Nintendo DS | melonDS | `.nds` | Owner-reported gameplay, game-specific audio/performance issues remain |
| Nintendo DSi | melonDS | `.dsi`, DSi-only `.nds` | Experimental, own firmware/NAND required, states unsupported |
| Nintendo 64 | Mupen64Plus-Next | `.z64`, `.v64`, `.n64` | Experimental, hardware performance unverified |
| Nintendo 3DS (test build 0.5.2+) | Dekopon v3.0.0-RC1 (unmodified release) | decrypted `.3ds`, `.cci`, `.cxi`, `.zcci`, `.zcxi` | Experimental, owner-reported gameplay; uses Dekopon's own menu and returns to Homebrew Menu on quit. Games in `/switch/dekopon/roms/` are listed too |

PS1 and PSP remain research topics. See [system documentation](docs/V0.3-SUPPORT.md), [N64 notes](docs/V0.3.3-N64.md) and [v0.4.0 validation](docs/V0.4.0-INTEGRATION.md) for firmware and integration details. Earlier version notes are historical records.

## Controls and saves

These are the v0.5.0 controls. Earlier releases use the v0.4 layout described in their notes.

| Action | Library control |
| --- | --- |
| Navigate | D-pad / left stick, or touch and drag |
| Play | A, or tap the selected game |
| Change tab (Home, Library, Settings) | L / R |
| Favourite | Y |
| Search | X |
| Change console | ZL / ZR |
| Change sort | − (Library) |
| Settings | + |
| Back | B (press twice on Home to close) |

| Action | In-game control |
| --- | --- |
| Open the menu (save slots, rewind, speed, cheats, display, quit) | + and − together, or tap the top-right corner |
| Quick save / quick load (Slot 1) | ZR / ZL (N64: hold − as well) |
| Fast forward | R3 (hold or toggle) |
| Rewind | Hold L3 (not on DS or N64) |

DS provides side-by-side, stacked and focused layouts, bottom-screen touch and docked virtual touch. Choosing **Save and quit** writes an automatic save, and **Resume** on Home continues from it.

Normal saves, save-state slots (with pictures), cheats and per-game settings live in the app's data folder, separated by console and game. Back up `saves/` before changing cores or moving ROM paths. State compatibility across core revisions is not guaranteed.

## Build and contribute

The frontend uses C++17, SDL2, libnx and devkitA64. Emulator sources and project build adaptations are included under `vendor/`. The recorded build used devkitA64 GCC 16.1.0 and SDL2 2.28.5.

From a configured devkitPro MSYS2 environment with Switch development packages, SDL2, curl, zlib and CMake installed, set `DEVKITPRO` to your SDK installation and run

```sh
bash scripts/build-multi-switch.sh
```

The main output is `AI1Emu.nro`. Companion NROs are generated under `build/switch/cores/`. Read [collaboration notes](COLLABORATING.md) and [contribution guidance](CONTRIBUTING.md) for the frontend, Windows preview, tests and update service.

The v0.5.0 app and all five companion NROs compile with devkitA64 GCC 16.1.0, and the host checks pass (`bash scripts/run-host-tests.sh`). Gameplay on a Switch has not been verified for v0.5.0; later test builds have owner-reported gameplay (see [known issues](docs/KNOWN-ISSUES.md)). Since 0.5.1 an optional **Performance boost** (off by default) raises the CPU to 1785 MHz, Nintendo's own maximum; since 0.5.13 it also applies to 3DS through a sys-clk profile (CPU 1785 MHz, memory 1600 MHz). Nothing runs above Nintendo's clock limits.

Game and console testing follows the [benchmark reporting guide](docs/BENCHMARKING.md). Standardized hardware measurements have not been published yet. Reports identify the build, device, settings and measurement method.

## Credits and licensing

AI1-owned frontend and integration code uses the scoped MIT terms in [LICENSE](LICENSE). Emulator cores and third-party components retain their own terms. The full collection is not uniformly MIT licensed. Snes9x and Genesis Plus GX include noncommercial restrictions. Preserve upstream notices and review applicable terms before redistributing a package. Branding and third-party metadata have separate terms.

Thanks to the mGBA, melonDS, FCEUmm, Snes9x, Genesis Plus GX, Mupen64Plus-Next, Libretro, devkitPro, libnx, SDL2 and font communities. See [licenses](licenses/) and upstream source trees for attribution.

3DS support uses **[Dekopon](https://github.com/PalindromicBreadLoaf/dekopon)** by PalindromicBreadLoaf, based on [Azahar](https://azahar-emu.org) (the continuation of Citra), with the NXVK graphics driver (a Mesa/NVK fork). AI1 Emu distributes the unmodified v3.0.0-RC1 release binary as a separate program under GPL-2.0-or-later; its hash and source are recorded in [Dekopon-NOTICE](licenses/Dekopon-NOTICE.txt). Box art downloaded in the app comes from [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails) (see [its notice](licenses/libretro-thumbnails-README.md)).

### Network use

Gameplay never needs the internet. When Wi-Fi is available the app contacts: the AI1 Emu update service on Cloudflare (update checks when the app opens, every 5 hours while open, and when you press the button), `raw.githubusercontent.com` (missing box art, requested by game file name), and the AI1 Emu cheat service (only when you search for cheats in a game). There are no accounts or analytics. Cover and cheat requests contain the game's title, and like any web request each one reveals your IP address to the service, which its hosting may log.

Supply your own legally obtained games and required firmware. Downloads exclude commercial ROMs, proprietary firmware, console keys and commercial cover collections. AI1 Emu is an independent homebrew project, unaffiliated with Nintendo or the original game publishers.
