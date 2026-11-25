// v0.5 library input: tabs, Home, Library grid, Settings and footer hints, by touch and controller.
#define SDL_MAIN_HANDLED
#include "library_ui.h"
#include "platform_library.h"
#include "ui_kit.h"
#include <SDL.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
int main() {
 SDL_SetMainReady();
 SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
 assert(SDL_Init(SDL_INIT_VIDEO) == 0);
 auto root = std::string("build/touch-") + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
 _putenv_s("AI1EMU_PREVIEW_ROOT", root.c_str());
 std::filesystem::create_directories(root);
 auto surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
 auto renderer = SDL_CreateSoftwareRenderer(surface);
 assert(renderer);
 {
  LibraryUI ui(renderer);
  std::vector<std::string> games;
  for (int i = 0; i < 9; ++i) games.push_back(root + "/roms/game-" + std::to_string(i) + ".gba");
  ui.setGames(games);
  auto tap = [&](int x, int y) {
   ui.draw(false); // hit areas come from the last drawn frame
   SDL_Event e{};
   e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y;
   ui.handlePointer(e);
   e.type = SDL_MOUSEBUTTONUP;
   return ui.handlePointer(e);
  };
  // Tabs: Home, Library, Settings across the top centre.
  assert(ui.tab == LibraryUI::TabHome);
  tap(645, 47); assert(ui.tab == LibraryUI::TabLibrary);
  tap(760, 47); assert(ui.tab == LibraryUI::TabSettings);
  tap(530, 47); assert(ui.tab == LibraryUI::TabHome);
  // Controller: L/R cycle, + toggles Settings.
  ui.update(pad::R, 0); assert(ui.tab == LibraryUI::TabLibrary);
  ui.update(pad::L, 0); ui.update(pad::L, 0); assert(ui.tab == LibraryUI::TabSettings);
  ui.update(pad::Plus, 0); assert(ui.tab == LibraryUI::TabHome);
  // Library grid: first tap selects, second tap launches.
  ui.tab = LibraryUI::TabLibrary;
  assert(tap(290, 240).type == LibraryUI::Command::None && ui.gridIndex == 1);
  auto launch = tap(290, 240);
  assert(launch.type == LibraryUI::Command::Launch && launch.path == ui.view[1] && !launch.resume);
  // D-pad moves through the grid (5 columns).
  ui.gridIndex = 0; ui.update(pad::Down, 0); assert(ui.gridIndex == 5);
  ui.update(pad::Right, 0); assert(ui.gridIndex == 6);
  // Search pill and footer hints.
  assert(tap(150, 122).type == LibraryUI::Command::Search);
  assert(ui.update(pad::A, 0).type == LibraryUI::Command::Launch);
  // Filters: ZR cycles All -> Favourites -> GBA.
  ui.update(pad::ZR, 0); assert(ui.filter == 1 && ui.view.empty());
  ui.update(pad::Y, 0); // nothing selected: no crash
  ui.update(pad::ZR, 0); assert(ui.filter == 2 && ui.view.size() == 9);
  ui.update(pad::Y, 0); assert(ui.favorites.size() == 1);
  // Offscreen console filters remain accessible without shoulder buttons.
  for(int i=0;i<15;++i){int previous=ui.filter;tap(1020,122);assert(ui.filter==(previous+1)%(2+int(platforms().size())));}
  // Settings: tap a section, then a row.
  ui.tab = LibraryUI::TabSettings;
  tap(180, 190); assert(ui.section == LibraryUI::SecGameplay && ui.sectionFocus);
  int before = ui.settings.gameplay.ffSpeed;
  ui.update(pad::Right, 0); assert(ui.settings.gameplay.ffSpeed == (before + 1) % 4);
  ui.update(pad::B, 0); assert(!ui.sectionFocus);
  ui.update(pad::Down, 0); assert(ui.section == LibraryUI::SecControls);
  ui.update(pad::A, 0); assert(ui.sectionFocus);
  ui.update(pad::X | pad::L, 0); assert(ui.tab == LibraryUI::TabSettings); // buttons are captured on Controls
  ui.update(pad::B, 0); assert(!ui.sectionFocus);
  // Home: B twice closes the app.
  ui.tab = LibraryUI::TabHome;
  assert(ui.update(pad::B, 0).type == LibraryUI::Command::None);
  assert(ui.update(pad::B, 0).type == LibraryUI::Command::Quit);
  // Synthetic mouse events from touch are ignored (touch arrives as finger events).
  SDL_Event e{};
  e.type = SDL_MOUSEBUTTONDOWN; e.button.which = SDL_TOUCH_MOUSEID; e.button.button = SDL_BUTTON_LEFT; e.button.x = 645; e.button.y = 47;
  ui.handlePointer(e); e.type = SDL_MOUSEBUTTONUP; ui.handlePointer(e);
  assert(ui.tab == LibraryUI::TabHome);
  ui.stopStorageScan();
 }
 SDL_DestroyRenderer(renderer);
 SDL_FreeSurface(surface);
 SDL_Quit();
 std::cout << "PASS: tabs, Home quit, grid select and launch, search, filters, favourites, settings rows, controls capture and synthetic-mouse suppression.\n";
 return 0;
}
