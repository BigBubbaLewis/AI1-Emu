#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$DEVKITPRO/msys2/mingw64/bin:$PATH"
CXX="$(command -v g++ || command -v c++)"; MINGW="$(dirname "$CXX")/.."
OUT=build/release-host;mkdir -p "$OUT"
FLAGS=(-std=c++17 -O1 -DRETROVERSE_WINDOWS_PREVIEW -Isource -I"$MINGW/include/SDL2")
LIBS=(-L"$MINGW/lib" -lSDL2 -lz)
for name in ui_kit app_data game_session online_cheats platform_library library_ui game_database;do
 "$CXX" "${FLAGS[@]}" -c "source/$name.cpp" -o "$OUT/$name.o"
done
UI=("$OUT/library_ui.o" "$OUT/ui_kit.o" "$OUT/app_data.o" "$OUT/game_database.o" "$OUT/platform_library.o")
SESSION=("$OUT/game_session.o" "$OUT/online_cheats.o" "$OUT/ui_kit.o" "$OUT/app_data.o" "$OUT/platform_library.o")
for t in touch_test yellow_artwork_test ui_storage_test;do "$CXX" "${FLAGS[@]}" "tests/$t.cpp" "${UI[@]}" "${LIBS[@]}" -o "$OUT/$t.exe";done
"$CXX" "${FLAGS[@]}" tests/game_session_test.cpp "${SESSION[@]}" "${LIBS[@]}" -o "$OUT/game_session_test.exe"
"$CXX" "${FLAGS[@]}" tests/online_cheats_test.cpp "$OUT/online_cheats.o" "${LIBS[@]}" -o "$OUT/online_cheats_test.exe"
for t in platform_test library_duplicates_test update_manifest_test core_list_test;do "$CXX" "${FLAGS[@]}" -DSDL_MAIN_HANDLED "tests/$t.cpp" "$OUT/platform_library.o" "${LIBS[@]}" -o "$OUT/$t.exe";done
for t in safe_file_test update_install_test ds_audio_test;do "$CXX" "${FLAGS[@]}" "tests/$t.cpp" "${LIBS[@]}" -o "$OUT/$t.exe";done
export PATH="$MINGW/bin:$PATH"
for t in touch_test yellow_artwork_test ui_storage_test game_session_test online_cheats_test platform_test library_duplicates_test update_manifest_test core_list_test safe_file_test update_install_test ds_audio_test;do echo "== $t";"$OUT/$t.exe";done
"$CXX" "${FLAGS[@]}" source/windows_preview.cpp "${UI[@]}" "$OUT/game_session.o" "$OUT/online_cheats.o" "${LIBS[@]}" -static-libgcc -static-libstdc++ -mwindows -o "$OUT/AI1Emu-Preview.exe"
for dll in SDL2.dll libwinpthread-1.dll zlib1.dll;do cp "$MINGW/bin/$dll" "$OUT/";done
mkdir -p "$OUT/romfs";cp romfs/* "$OUT/romfs/"
echo 'PASS: final-source release host suite and Windows preview compilation'
