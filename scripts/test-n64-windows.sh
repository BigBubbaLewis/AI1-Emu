#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Run in MSYS2 after installing an x86-64 C++ compiler, SDL2 and NASM.
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/devkitPro/msys2/mingw64/bin:$PATH"
mkdir -p build/n64-host-source build/n64-host-system build/n64-host-saves
# Host and Switch objects must never be mixed; copy source only.
tar -C vendor/mupen64plus-next --exclude='*.o' --exclude='*.a' --exclude='*.d' --exclude='*.dll' --exclude='.git' -cf - . | tar -C build/n64-host-source -xf -
make -C build/n64-host-source platform=win ARCH=x86_64 WITH_DYNAREC=x86_64 \
 ASFLAGS='-O0 -f win64 -d WIN64' HAVE_PARALLEL_RDP=0 HAVE_PARALLEL_RSP=0 HAVE_THR_AL=0 -j4
# Set PYTHON to an installed Python executable if necessary.
"${PYTHON:-python3}" tests/make_n64_diagnostic.py
g++ -std=c++17 -O2 -Isource -IdevkitPro/msys2/mingw64/include/SDL2 \
 -Ivendor/mupen64plus-next/libretro-common/include tests/n64_core_smoke.cpp source/n64_video.cpp \
 -LdevkitPro/msys2/mingw64/lib -lSDL2 -lopengl32 -static-libgcc -static-libstdc++ -o build/n64-core-smoke.exe
./build/n64-core-smoke.exe
