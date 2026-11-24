# AI1 Emu source and update service

Worker: `ai1emu-updates`.
Public hub: https://ai1emu-updates.freegamez.workers.dev/
Source browser: https://ai1emu-updates.freegamez.workers.dev/developer/

The owner authorized publication of the complete app source on November 3, 2026.
The site hosts C++ code, build/artwork tooling, Cloudflare code, notices,
complete corresponding-source archives and ready-to-install packages. No ROMs,
proprietary BIOS, console keys, commercial covers, credentials or private deployment
sessions are public assets. Browser source edits download a local copy; the
Switch app requires devkitPro compilation. See `../COLLABORATING.md`.

## Publish

1. Increase the app/Makefile version, build and test.
2. Run `bash scripts/create-source-archive.sh`, then
   `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package-v03.ps1`.
3. Run `node cloudflare/scripts/prepare-collaboration.mjs`. This stages the public
   snapshot and private upload manifest, retaining stable GBA assets. A fresh
   source extraction bootstraps stable assets from the public service.
4. With authorized Cloudflare account access, run `wrangler deploy` in this folder.
   Alternatively register an assets-upload-session through the connected API,
   save its JWT privately in `.deploy-private/session.json`, run
   `scripts/upload-assets.mjs`, and upload `src/worker.mjs` with ASSETS binding
   and completion token, matching wrangler.jsonc.
5. Independently download and verify the live NRO, source and package.

`npm test` checks channels, read-only behavior, source privacy and streaming.
`npm run check` checks types. Configuration uses date 2026-11-03, nodejs_compat,
ASSETS, run_worker_first and logs. No permanent credential is stored here.

Large archives use internal asset parts below 25 MiB and stream as one download.
The Worker enforces length and exposes the SHA-256 ETag without buffering an
entire archive. No R2 is required. Complete GET and HEAD are supported;
range/resume is not implemented.

`/latest.json` examines `AI1Emu/<version>` User-Agent. v0.3.1+ multi-core clients
receive `/latest-multicore.json`; older clients retain stable v0.2.2 GBA-only.
v0.3.2 updates the main app and retains the working v0.3.1 companions.
Fresh installations need the full package.

Never replace published version bytes. Preparation rejects a different staged
archive/NRO at an existing version. Credentials are excluded by explicit source
allowlists, not only gitignore.
