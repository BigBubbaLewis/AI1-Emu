#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/devkitPro/msys2/mingw64/bin:$PATH"
mkdir -p Windows-Preview/romfs Windows-Preview/data/artwork-cache Windows-Preview/data/roms Windows-Preview/data/metadata Windows-Preview/data/saves Windows-Preview/data/covers Windows-Preview/data/screenshots
g++ -std=c++17 -O2 -DRETROVERSE_WINDOWS_PREVIEW -IdevkitPro/msys2/mingw64/include/SDL2 \
 source/windows_preview.cpp source/library_ui.cpp source/library_touch.cpp source/game_database.cpp source/platform_library.cpp \
 -LdevkitPro/msys2/mingw64/lib -lSDL2 -static-libgcc -static-libstdc++ -mwindows -o Windows-Preview/AI1Emu-Preview.exe
cp devkitPro/msys2/mingw64/bin/SDL2.dll Windows-Preview/
cp devkitPro/msys2/mingw64/bin/libwinpthread-1.dll Windows-Preview/
cp romfs/* Windows-Preview/romfs/
cp assets/artwork-development/artwork-cache/*.png Windows-Preview/data/artwork-cache/
cp assets/artwork-development/artwork-cache/*.source.txt Windows-Preview/data/artwork-cache/




