# AI1 Emu — build, edit and publish

Project hub: https://ai1emu-updates.freegamez.workers.dev/
Source browser: https://ai1emu-updates.freegamez.workers.dev/developer/

The complete source download contains the actual C++ app, emulator trees with
AI1 build changes, SDL assets, artwork tools, tests, license notices, dependency
source archives, and the Cloudflare update/download service. It excludes local
credentials, user ROMs, proprietary firmware, commercial cover caches, SDK
executables, build outputs and private upload-session files.

## Editing the app

Extract the complete source into a working folder. UI: `source/library_ui.cpp`
and `source/library_touch.cpp`. Main mGBA frontend: `source/main.cpp`. Other
cores: `source/core_runner.cpp`. Console scanning/saves: `platform_library.cpp`.
Folder merge: `legacy_migration.cpp`. Windows cover tools: `tools/`.

Install devkitPro's Switch development packages (`switch-dev`, libnx, SDL2,
switch-curl, switch-zlib and their graphics dependencies) and CMake. The existing
build was produced with devkitA64 GCC 16.1.0 and SDL2 2.28.5. In the devkitPro
MSYS2 environment, set `DEVKITPRO` to the installed SDK root, then run:

```
bash scripts/build-multi-switch.sh
```

Do not re-download mGBA: the exact source is included in `vendor/mgba`. Build
one modified core at a time. No overclocking is required or configured. Build
success does not verify Switch gameplay. Keep a working NRO and SD-save backup.
See `docs/V0.3-SUPPORT.md` for console formats, firmware and hardware limitations.

The Windows UI preview uses the same renderer. Its build script expects MinGW
SDL2 under `devkitPro/msys2/mingw64`; adjust that path for your local installation.
It previews UI, not Windows gameplay. Native host tests are in `tests/`, including
folder merging and DS startup using a self-authored synthetic cartridge.

## Editing Cloudflare

Cloudflare dashboard → Workers & Pages → `ai1emu-updates` → Edit code edits the
web/update Worker. C++ source files are downloadable assets, not code the Worker
can compile. The web source editor downloads an edited local copy; it does not
silently change the published app or grant public write access.

To publish a collaborator's changes, grant that person appropriate access to
your Cloudflare account. Do not share passwords, console keys or permanent API
tokens in source. A local authorized maintainer can use Wrangler login:

```
cd cloudflare
npm ci
npm test
npx wrangler types
npm run check
```

After modifying the Switch app, increase its version in `source/app_version.h`
and the Makefile, build, and verify. From the project root:

```
bash scripts/create-source-archive.sh
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package-v03.ps1
node cloudflare/scripts/prepare-collaboration.mjs
cd cloudflare
npx wrangler login
npx wrangler deploy
```

The preparation script stages the complete source, package, raw project files,
source-browser pages and versioned NRO. Large archives are split internally into
assets below Cloudflare's per-file limit, then streamed as ordinary single-file
downloads. No R2 bucket, public upload endpoint or database is required. Previous
stable GBA assets are retained; a fresh checkout fetches those public assets from
the current service before deployment. Do not change already published version
paths to different bytes: increase the version instead.

`/latest.json` uses the app's versioned User-Agent: v0.3.1+ installations get the
multi-console main-app channel; older GBA-only clients retain v0.2.2. This avoids
offering an incomplete multi-core installation to old clients. New installations
must use the full package. The v0.3.2 merge update deliberately retains the
working v0.3.1 companion binaries.

The connected-tool deployment used in this workspace registers an asset-upload
session and uses `cloudflare/scripts/upload-assets.mjs`. Its short-lived session
and completion tokens stay in `.deploy-private/` and are never source assets.

## Licenses and user data

AI1-owned frontend/integration code is MIT-licensed (see `licenses/AI1-MIT.txt`).
Bundled third-party code retains its own licenses. The complete emulator package
is noncommercial because Snes9x and Genesis Plus GX impose noncommercial terms.
Preserve all notices and corresponding source when redistributing. Logos and
branding are not covered by the code license. No commercial game covers are
cleared for bundling; cover downloads remain personal caches.

The folder merge checks copies byte-for-byte, remaps saves when the same ROM is
already in a console subfolder, and preserves differing older files under
`ai1emu/legacy-conflicts/`. It removes `retroverse` only once empty. The report is
`ai1emu/migration-report.txt`. Do not interrupt SD writes; a failed copy retains
its source and can be retried on a later launch.

N64 integration (v0.3.3): source/n64_runner.cpp and source/n64_video.cpp. Its Switch core source is vendor/mupen64plus-next with pinned provenance and local JIT/exit patches. See docs/V0.3.3-N64.md. The four earlier companion binaries are preserved byte-for-byte in this release; shared platform IDs 0–9 are unchanged.


DS audio runner (v0.3.5): source/ds_runner.cpp and source/ds_audio.h. The generic core_runner.cpp continues serving NES/SNES/Sega. Byte-identical library grouping is in platform_library.cpp; GB/GBC database/cache fallback is in game_database.cpp and library_ui.cpp. DS audio component installs through Settings and records cores/melonds-audio.version.

