#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVKITPRO="${DEVKITPRO:-$PWD/devkitPro}"
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"
# Interpreter/software rendering: no ARM JIT or OpenGL dependency for melonDS.
make -C vendor/melonds-libretro platform=libnx HAVE_OPENGL=0 -j4
make build/switch/cores/melonds.nro
make -C vendor/fceumm -f Makefile.libretro platform=libnx -j4
make build/switch/cores/fceumm.nro
make -C vendor/snes9x/libretro platform=libnx -j4
make build/switch/cores/snes9x.nro
make -C vendor/genesis-plus-gx -f Makefile.libretro platform=libnx -j4
make build/switch/cores/genesis-plus-gx.nro
make -C vendor/mupen64plus-next platform=libnx -j4
make build/switch/cores/mupen64plus-next.nro
bash scripts/build-switch.sh
