#!/usr/bin/env bash
# Builds the desktop UI preview (library, in-game menu and messages) with MinGW.
# Usage: bash scripts/build-windows-preview.sh [output-folder]   (default: Windows-Preview)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-Windows-Preview}"
export PATH="$PWD/devkitPro/msys2/mingw64/bin:${DEVKITPRO:-/opt/devkitpro}/msys2/mingw64/bin:$PATH"
HOST_CXX="${CXX:-$(command -v g++ || command -v c++)}"
MINGW="$(dirname "$HOST_CXX")/.."
mkdir -p "$OUT/romfs" "$OUT/data/artwork-cache" "$OUT/data/roms" "$OUT/data/saves" "$OUT/screenshots"
"$HOST_CXX" -std=c++17 -O2 -DRETROVERSE_WINDOWS_PREVIEW -I"$MINGW/include/SDL2" \
 source/windows_preview.cpp source/library_ui.cpp source/ui_kit.cpp source/app_data.cpp source/game_session.cpp source/online_cheats.cpp \
 source/game_database.cpp source/platform_library.cpp \
 -L"$MINGW/lib" -lSDL2 -lz -static-libgcc -static-libstdc++ -mwindows -o "$OUT/AI1Emu-Preview.exe"
cp "$MINGW/bin/SDL2.dll" "$OUT/"
for dll in libwinpthread-1.dll zlib1.dll; do [ -f "$MINGW/bin/$dll" ] && cp "$MINGW/bin/$dll" "$OUT/"; done
cp romfs/* "$OUT/romfs/"
if [ -d assets/artwork-development/artwork-cache ]; then
 cp assets/artwork-development/artwork-cache/*.png "$OUT/data/artwork-cache/" 2>/dev/null || true
fi
echo "Built $OUT/AI1Emu-Preview.exe (run it from $OUT; --capture tours every screen)"
