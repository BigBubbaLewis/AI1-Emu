# Working on AI1 Emu

The public repository is the collaboration source. Preserve existing UI, branding, working emulator behavior and saves unless the user requests a change.

For a requested repository update, update the README, screenshots and feature/support documentation to match the delivered build. Use actual application captures and identify the version and capture environment. Preserve stable versus preview channel distinctions. Repository pushes do not authorize changing a release channel or overwriting an immutable versioned download.

Game and console benchmark reports follow `docs/BENCHMARKING.md`. Never invent FPS, timings, compatibility results or hardware verification. Separate compilation, host tests, subjective owner reports and measured physical Switch tests. Record the exact build and settings for comparisons.

Preserve third-party licenses and source. Exclude ROMs, proprietary firmware, console keys, credentials, SDK installations and private artwork caches from commits. The frontend's MIT scope does not relicense bundled emulator cores or branding.

Make focused changes, run relevant existing checks, and report what remains unverified. Cloudflare tests run with `node --test test/*.test.mjs` from `cloudflare/`. Switch builds and core changes require the installed devkitPro toolchain. Use the documented scripts and keep the working package available for rollback.
