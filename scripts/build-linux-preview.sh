#!/usr/bin/env bash
# Optional: build the real UI preview on Linux without a window and capture every
# v0.4 screen. Uses the bundled SDL2 2.28.5 with only the offscreen/dummy drivers.
# Put sample files in build/linux-preview/run/data/roms/<console>/ (or set
# AI1EMU_PREVIEW_ROOT) to preview a populated library.
set -euo pipefail
cd "$(dirname "$0")/.."
out="$PWD/build/linux-preview"; mkdir -p "$out"
if [ ! -f "$out/sdl2/lib/libSDL2.a" ]; then
  tar -xzf build/v03-dependency-sources/SDL2-2.28.5.tar.gz -C "$out"
  (cd "$out/SDL2-2.28.5" && ./configure --prefix="$out/sdl2" --disable-shared --enable-static \
    --disable-audio --disable-joystick --disable-haptic --disable-sensor --disable-hidapi \
    --disable-video-x11 --disable-video-wayland --disable-video-kmsdrm --disable-video-opengl \
    --disable-video-opengles --disable-video-opengles1 --disable-video-opengles2 --disable-video-vulkan \
    --enable-video-offscreen --enable-video-dummy --disable-libudev --disable-dbus --disable-ime >/dev/null \
    && make -j"$(nproc)" >/dev/null && make install >/dev/null)
fi
g++ -std=c++17 -O2 -DRETROVERSE_WINDOWS_PREVIEW -I"$out/sdl2/include/SDL2" \
  source/windows_preview.cpp source/library_ui.cpp source/library_touch.cpp source/game_database.cpp source/platform_library.cpp \
  "$out/sdl2/lib/libSDL2.a" -lm -lpthread -ldl -o "$out/AI1Emu-Preview"
mkdir -p "$out/run/data/roms" && rm -rf "$out/run/romfs" && cp -r romfs "$out/run/romfs"
cd "$out/run" && rm -rf screenshots && SDL_VIDEODRIVER=offscreen ../AI1Emu-Preview --capture && ls screenshots
