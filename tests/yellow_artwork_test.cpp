#define SDL_MAIN_HANDLED
#include "library_ui.h"
#include "platform_library.h"
#include "game_database.h"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <iostream>
namespace fs = std::filesystem;
int main() {
 SDL_SetMainReady();
 SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
 assert(SDL_Init(SDL_INIT_VIDEO) == 0);
 std::string root = "build/yellow-fixture-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
 _putenv_s("AI1EMU_PREVIEW_ROOT", root.c_str());
 std::string title = "Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced)";
 auto rom = root + "/roms/" + title + ".gb";
 fs::create_directories(root + "/roms");
 std::vector<char> header(512);
 header[0x143] = char(0x80);
 std::ofstream(rom, std::ios::binary).write(header.data(), header.size());
 fs::create_directories(root + "/artwork-cache/gb");
 fs::copy_file("romfs/app-icon.png", root + "/artwork-cache/gb/" + title + ".png");
 fs::create_directories(root + "/artwork-cache/gbc");
 std::ofstream(root + "/artwork-cache/gbc/" + title + ".png") << "invalid cached image";
 assert(detectPlatform(rom) == Platform::GBC);
 assert(gameTitle(rom) == title);
 auto surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
 assert(surface);
 auto renderer = SDL_CreateSoftwareRenderer(surface);
 assert(renderer);
 {
  LibraryUI ui(renderer);
  ui.setGames({rom});
  ui.draw(false);
  auto key = root + "/artwork-cache/gb/" + title;
  assert(ui.images.count(key));
  // Covers decode on a background thread; the first frame shows a placeholder.
  for (int i = 0; i < 500 && !ui.artIdle(); ++i) { SDL_Delay(4); ui.draw(false); }
  assert(ui.artIdle());
  assert(ui.images[key].texture && ui.images[key].state == 1);
  ui.tab = LibraryUI::TabSettings;
  ui.section = LibraryUI::SecMap;
  ui.draw(false);
  ui.draw(false);
  assert(ui.mapState == 2 && ui.mapOwned.size() == 1);
  ui.setGames({rom, rom});
  ui.draw(false);
  assert(ui.mapOwned.size() == 1);
  // Missing covers: a 3DS game is looked up on libretro-thumbnails by its file name;
  // the GB game already has its cover; a "no cover online" note stops further requests.
  {
   auto n3ds = root + "/roms/Pokemon Y (USA) (En,Ja,Fr,De,Es,It,Ko).3ds";
   std::ofstream(n3ds, std::ios::binary) << "3ds";
   ui.setGames({rom, n3ds});
   std::vector<std::pair<std::string, std::string>> jobs;
   for (int i = 0; i < 4; ++i) for (auto& j : ui.missingCovers(1)) jobs.push_back(j);
   assert(jobs.size() == 1);
   assert(jobs[0].first == "https://raw.githubusercontent.com/libretro-thumbnails/Nintendo_-_Nintendo_3DS/master/Named_Boxarts/Pokemon%20Y%20%28USA%29%20%28En%2CJa%2CFr%2CDe%2CEs%2CIt%2CKo%29.png");
   assert(jobs[0].second == root + "/artwork-cache/3ds/Pokemon Y (USA) (En,Ja,Fr,De,Es,It,Ko)");
   fs::create_directories(root + "/artwork-cache/3ds");
   std::ofstream(jobs[0].second + ".none") << "none";
   ui.setGames({rom, n3ds});
   assert(ui.missingCovers(10).empty());
   ui.coverArrived(key); // reloading a cover is safe and draws again
   ui.draw(false);
  }
  ui.stopStorageScan();
 }
 SDL_DestroyRenderer(renderer);
 SDL_FreeSurface(surface);
 SDL_Quit();
 std::cout << "PASS: enhanced GB header selects GBC while Yellow resolves GB database and renders cached GB cover in actual UI\n";
 return 0;
}
