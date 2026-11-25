// v0.5 in-game session: slots, quick save, overwrite confirmation, undo, rewind,
// fast forward, cheats, per-game settings, play history and the menu drawing.
#define SDL_MAIN_HANDLED
#include "game_session.h"
#include "app_data.h"
#include "ui_kit.h"
#include <SDL.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
int main() {
 SDL_SetMainReady();
 SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
 assert(SDL_Init(SDL_INIT_VIDEO) == 0);
 std::string root = "build/session-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + "/";
 fs::create_directories(root + "roms/gba");
 std::string game = root + "roms/gba/Legend of Zelda, The - The Minish Cap (USA).gba";
 std::string prefix = gameSavePrefix(root, game);
 fs::create_directories(fs::path(prefix).parent_path());
 auto surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
 auto renderer = SDL_CreateSoftwareRenderer(surface);
 assert(renderer);

 // Pure helpers
 assert(ui::prettyTitle("Legend of Zelda, The - The Minish Cap (USA)") == "The Legend of Zelda - The Minish Cap");
 assert(ui::prettyTitle("Metroid Fusion (USA)") == "Metroid Fusion");
 assert(formatPlaytime(0) == "Not played yet" && formatPlaytime(3600 * 14 + 60 * 20) == "14 hrs 20 min" && formatPlaytime(59 * 60, true) == "59 min");
 {
  std::vector<Cheat> c = {{"Infinite money", "82025BC4 423F", true}, {"Walk through walls", "509197D3+0FA1B5E8", false}};
  assert(saveCheats(root + "cheats/test.cht", c));
  auto back = loadCheats(root + "cheats/test.cht");
  assert(back.size() == 2 && back[0].enabled && !back[1].enabled && back[1].code == "509197D3+0FA1B5E8" && back[0].description == "Infinite money");
 }

 {
  std::vector<unsigned char> state(4096, 1);
  int resets = 0, cheatCalls = 0;
  CoreHooks hooks;
  hooks.serializeSize = [&] { return state.size(); };
  hooks.serialize = [&](void* d, size_t n) { std::memcpy(d, state.data(), n); return true; };
  hooks.unserialize = [&](const void* d, size_t n) { if (n != state.size()) return false; std::memcpy(state.data(), d, n); return true; };
  hooks.reset = [&] { ++resets; };
  hooks.cheatReset = [&] {};
  hooks.cheatSet = [&](unsigned, bool, const char*) { ++cheatCalls; };
  int boostCalls = 0;
  bool boostOn = false;
  SessionOptions options;
  options.setBoost = [&](bool on) { ++boostCalls; boostOn = on; };
  GameSession s(renderer, root, game, Platform::GBA, prefix, "", hooks, options);
  assert(boostCalls == 0); // performance boost is off by default
  std::vector<Uint32> pixels(240 * 160, 0xFF336699u);
  s.captureFrame(pixels.data(), 240, 160, 240 * 4, 1);

  // Quick save with ZR writes slot 1 (the v0.4 ".state" file) and its picture.
  assert(s.update(pad::ZR, pad::ZR));
  assert(fs::exists(prefix + ".state") && fs::exists(prefix + ".state.bmp"));
  state.assign(state.size(), 2);
  assert(s.update(pad::ZL, pad::ZL) && state[0] == 1); // quick load

  // Menu: + and - together open it; the game does not advance while it is open.
  assert(s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus) && s.menuOpen() && s.framesThisLoop() == 0);
  s.update(0, pad::Down); s.update(0, pad::A);               // Save state page, slot 1 focused
  s.update(0, pad::A);                                        // slot 1 is used: asks first
  state.assign(state.size(), 3);
  s.update(0, pad::A);                                        // confirm replace
  s.drawMenu(nullptr);
  s.update(0, pad::B); s.update(0, pad::Down); s.update(0, pad::A); // Load state page
  state.assign(state.size(), 9);
  s.update(0, pad::Right); s.update(0, pad::A);               // load slot 1 -> closes the menu
  assert(!s.menuOpen() && state[0] == 3);
  s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus);   // reopen, undo the load with X
  s.update(0, pad::Down); s.update(0, pad::Down); s.update(0, pad::A); s.update(0, pad::X);
  assert(state[0] == 9);
  s.update(0, pad::B); s.update(0, pad::B);
  assert(!s.menuOpen());

  // Fast forward: hold R3 (default 2x).
  assert(s.update(pad::RStick, pad::RStick) && s.fastForward() && s.framesThisLoop() == 2 && s.audioScale() == 0.f);
  s.update(0, 0);
  assert(!s.fastForward() && s.framesThisLoop() == 1);

  // Rewind: snapshots every 4 frames; holding L3 steps back through them.
  for (int i = 0; i < 40; ++i) { state[0] = (unsigned char)(100 + i); s.afterFrame(); }
  assert(s.update(pad::LStick, pad::LStick) && s.rewinding() && state[0] < 140);
  unsigned char first = state[0];
  s.update(pad::LStick, 0);
  assert(state[0] < first);
  s.update(0, 0);
  assert(!s.rewinding());

  // Restart asks first, then resets.
  s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus);
  for (int i = 0; i < 9; ++i) s.update(0, pad::Down);
  s.update(0, pad::A); s.update(0, pad::A);
  assert(resets == 1 && !s.menuOpen());

  // Per-game settings: Speed page, "This game", then change the speed.
  s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus);
  for (int i = 0; i < 4; ++i) s.update(0, pad::Down);
  s.update(0, pad::A); s.update(0, pad::Left);                 // apply to: This game
  s.update(0, pad::Down); s.update(0, pad::Right);              // 2x -> 3x
  s.update(0, pad::B); s.update(0, pad::B);                     // close: saves
  assert(hasGameOverrides(prefix) && loadGameplay(root, prefix).ffSpeed == 2 && loadSettings(root).gameplay.ffSpeed == 1);

  // Performance boost: Speed page, last row, switches the CPU clock right away.
  s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus);
  for (int i = 0; i < 4; ++i) s.update(0, pad::Down);
  s.update(0, pad::A);
  for (int i = 0; i < 6; ++i) s.update(0, pad::Down);
  s.update(0, pad::A);
  assert(boostOn && boostCalls == 1);
  s.update(0, pad::A);
  assert(!boostOn && boostCalls == 2);
  s.update(0, pad::B); s.update(0, pad::B);

  // Online lookup fails gracefully in a host preview; menu and save operations stay responsive.
  s.update(pad::Plus | pad::Minus,pad::Plus | pad::Minus);
  for(int i=0;i<5;++i)s.update(0,pad::Down);
  s.update(0,pad::A);s.update(0,pad::Down);s.update(0,pad::Down);s.update(0,pad::A);
  for(int i=0;i<20;++i){s.update(0,0);SDL_Delay(1);}
  assert(s.menuOpen());s.drawMenu(nullptr);s.update(0,pad::B);s.update(0,pad::B);assert(!s.menuOpen());

  // Quit saves the automatic slot and records play time.
  s.update(pad::Plus | pad::Minus, pad::Plus | pad::Minus);
  s.update(0, pad::Up);
  s.drawMenu(nullptr);
  bool keepGoing = s.update(0, pad::A);                         // "Save and quit"
  assert(!keepGoing);
  s.finish(true);
  assert(fs::exists(slotStatePath(prefix, 0)) && slotTime(prefix, 0) > 0 && usedSlots(prefix) == 2);
  assert(loadHistory(root).count(game) == 1);
  (void)cheatCalls;
 }
 SDL_DestroyRenderer(renderer);
 SDL_FreeSurface(surface);
 SDL_Quit();
 std::cout << "PASS: quick save/load, slot overwrite confirmation, load undo, fast forward, rewind, restart, per-game settings, auto save and play history\n";
 return 0;
}
