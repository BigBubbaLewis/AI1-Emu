#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVKITPRO="${DEVKITPRO:-$PWD/devkitPro}"
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"
cmake -S vendor/mgba -B build/mgba -G 'Unix Makefiles' \
 -DCMAKE_TOOLCHAIN_FILE="$PWD/vendor/mgba/src/platform/switch/CMakeToolchain.txt" \
 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF \
 -DBUILD_SHARED=OFF -DBUILD_STATIC=OFF -DM_CORE_GBA=ON -DM_CORE_GB=ON \
 -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF \
 -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF -DUSE_JSON_C=OFF -DUSE_FREETYPE=OFF \
 -DUSE_LZMA=OFF -DUSE_DISCORD_RPC=OFF -DENABLE_SCRIPTING=OFF -DBUILD_LTO=OFF
cmake --build build/mgba --target mgba_libretro -j4
make -j4

