# Publishing v0.5 (preview) — checklist

Use this when moving a 0.5.x test build to the public channel. Public channels: stable 0.3.5
and UI preview 0.4.0 today. Keep the stable/preview distinction: v0.5 goes out as a
**preview**, labelled experimental, not as the stable channel.

## Before publishing

- [ ] The owner has used the build for about a week with no crash, freeze or save problem.
- [ ] Game saves confirmed on a Switch after the 0.5.17 save fix (save in a game, quit,
      reopen, check the progress), on GBA and at least one other console.
- [ ] `bash scripts/run-host-tests.sh` passes on the exact commit being published.
- [ ] Clean build of the app and every emulator from that commit (`make OUT=...` after
      deleting the object folder), symbols archived for the version.
- [ ] Corresponding source: `SOURCE_ARCHIVE_DIR=<folder> bash scripts/create-source-archive.sh`
      from the same commit. 0.5.17's archive: `AI1-Emu-v0.5.17-Corresponding-Source.tar.gz`.
- [ ] The branch is pushed to GitHub (the source must be public once binaries are).
- [ ] README version notes, [known issues](KNOWN-ISSUES.md) and screenshots match the build.
- [ ] Dekopon: unmodified release binary, hash and source recorded in
      `licenses/Dekopon-NOTICE.txt`, GPL text in `licenses/Dekopon-GPL-2.0.txt`.

## Publishing

- [ ] Build the complete Switch package with the package script (it bundles every licence
      and the source archive).
- [ ] Deploy to the **live** `ai1emu-updates` Worker only from the workspace that holds the
      complete live asset set. Deploying from a machine without it removes the older
      downloads (0.3.5, 0.4.0). Do not publish 0.5.x by redeploying from the test-channel PC.
- [ ] Add 0.5.x as a new preview entry; leave the 0.3.5 and 0.4.0 downloads and their
      versioned URLs untouched.
- [ ] Download every published file back and compare SHA-256 with the build.

## Announcing (forums, Reddit, YouTube contacts)

- Say it is an **experimental preview**, link the [known issues](KNOWN-ISSUES.md), and ask
  for bug reports with the app's `logs/app.log`.
- Credit the emulators it builds on (mGBA, melonDS, Dekopon/Azahar, Mupen64Plus-Next,
  FCEUmm, Snes9x, Genesis Plus GX) and link the source.
- Make no performance claims that are not in `docs/benchmarks/`.
- No ROMs, firmware, keys or links to them; no Nintendo logos. Follow each site's rules on
  homebrew posts.
