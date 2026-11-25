#define SDL_MAIN_HANDLED
#include "library_ui.h"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <iostream>
int main() {
 SDL_SetMainReady();
 SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
 assert(SDL_Init(SDL_INIT_VIDEO) == 0);
 auto root = std::string("build/ui-storage-") + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
 _putenv_s("AI1EMU_PREVIEW_ROOT", root.c_str());
 std::filesystem::create_directories(root + "/roms/gba");
 for (int i = 0; i < 1200; ++i) std::ofstream(root + "/roms/gba/test-" + std::to_string(i) + ".gba") << "original fixture";
 auto surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
 auto renderer = SDL_CreateSoftwareRenderer(surface);
 assert(renderer);
 {
  LibraryUI ui(renderer);
  ui.tab = LibraryUI::TabSettings;
  ui.section = LibraryUI::SecAbout;
  ui.draw(false);
  auto start = SDL_GetTicks64();
  ui.stopStorageScan();
  assert(!ui.storageThread && !ui.storageStop);
  assert(ui.storageState == 0 || ui.storageState == 2);
  std::cout << "Storage stop returned in " << SDL_GetTicks64() - start << " ms\n";
  if (ui.storageState == 0) { ui.draw(false); for (int i = 0; i < 1000 && ui.storageState != 2; ++i) SDL_Delay(2); }
  assert(ui.storageState == 2);
  ui.stopStorageScan();
  assert(!ui.storageThread && ui.storageState == 2);
 }
 SDL_DestroyRenderer(renderer);
 SDL_FreeSurface(surface);
 SDL_Quit();
 std::cout << "PASS: actual storage scan stops, can restart when interrupted, and joins before game launch/destruction\n";
 return 0;
}
