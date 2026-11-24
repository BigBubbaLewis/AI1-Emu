#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/^#define AI1EMU_VERSION "\([0-9.]*\)"/\1/p' source/app_version.h)
test -n "$version"
mkdir -p build
output="build/AI1-Emu-v${version}-Corresponding-Source.tar.gz"
tar -czf "$output" --exclude='*.o' --exclude='*.obj' --exclude='*.a' \
 --exclude='*.d' --exclude='*.elf' --exclude='*.nro' --exclude='*.nacp' \
 --exclude='*.exe' --exclude='*.dll' --exclude='.git' \
 --exclude='vendor/mgba/cinema' source romfs scripts tests docs licenses tools \
 Makefile README.md COLLABORATING.md .gitignore \
 vendor/mgba vendor/melonds-libretro vendor/fceumm vendor/snes9x \
 vendor/genesis-plus-gx vendor/mupen64plus-next build/v03-dependency-sources \
 cloudflare/src cloudflare/scripts cloudflare/test cloudflare/site \
 cloudflare/README.md cloudflare/wrangler.jsonc cloudflare/package.json \
 cloudflare/package-lock.json cloudflare/tsconfig.json cloudflare/worker-configuration.d.ts
tar -tzf "$output" > "build/source-${version}-contents.txt"
printf 'Created and verified %s\n' "$output"
