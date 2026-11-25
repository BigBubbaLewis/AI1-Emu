#!/usr/bin/env bash
# Builds and runs the desktop (host) tests with MinGW. Run from any folder:
#   bash scripts/run-host-tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/devkitPro/msys2/mingw64/bin:${DEVKITPRO:-/opt/devkitpro}/msys2/mingw64/bin:$PATH"
HOST_CXX="${CXX:-$(command -v g++ || command -v c++)}"
MINGW="$(dirname "$HOST_CXX")/.."
OUT=build/tests
mkdir -p "$OUT"
CXX=("$HOST_CXX" -std=c++17 -O1 -DRETROVERSE_WINDOWS_PREVIEW -Isource -I"$MINGW/include/SDL2")
UI=(source/library_ui.cpp source/ui_kit.cpp source/app_data.cpp source/game_database.cpp source/platform_library.cpp)
LIBS=(-L"$MINGW/lib" -lSDL2 -lz)
run() { echo "== $1"; "$OUT/$1.exe"; }
"${CXX[@]}" tests/touch_test.cpp "${UI[@]}" "${LIBS[@]}" -o "$OUT/touch_test.exe"
"${CXX[@]}" tests/yellow_artwork_test.cpp "${UI[@]}" "${LIBS[@]}" -o "$OUT/yellow_artwork_test.exe"
"${CXX[@]}" tests/ui_storage_test.cpp "${UI[@]}" "${LIBS[@]}" -o "$OUT/ui_storage_test.exe"
"${CXX[@]}" tests/game_session_test.cpp source/game_session.cpp source/online_cheats.cpp source/ui_kit.cpp source/app_data.cpp source/platform_library.cpp "${LIBS[@]}" -o "$OUT/game_session_test.exe"
for t in platform_test library_duplicates_test update_manifest_test core_list_test; do
 "${CXX[@]}" -DSDL_MAIN_HANDLED tests/$t.cpp source/platform_library.cpp -L"$MINGW/lib" -lSDL2 -o "$OUT/$t.exe"
done
cp "$MINGW/bin/SDL2.dll" "$OUT/" 2>/dev/null || true
for dll in zlib1.dll libwinpthread-1.dll; do [ -f "$MINGW/bin/$dll" ] && cp "$MINGW/bin/$dll" "$OUT/"; done
export PATH="$PWD/$OUT:$PATH"
for t in touch_test yellow_artwork_test ui_storage_test game_session_test platform_test library_duplicates_test update_manifest_test core_list_test; do run $t; done
for t in safe_file_test update_install_test ds_audio_test; do
 "${CXX[@]}" tests/$t.cpp "${LIBS[@]}" -o "$OUT/$t.exe"
 run "$t"
done
"${CXX[@]}" tests/sysclk_profile_test.cpp source/sysclk_profile.cpp -o "$OUT/sysclk_profile_test.exe"
run sysclk_profile_test
echo "All host tests passed."
