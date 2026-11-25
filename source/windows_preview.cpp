// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Desktop preview of the real v0.5 interface (library, in-game menu and on-screen
// messages) using the same SDL2 drawing code as the Switch app. Games are not
// emulated: a generated picture stands in for the game screen.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "app_version.h"
#include "game_database.h"
#include "game_session.h"
#include "keyboard.h"
#include "library_ui.h"
#include "platform_library.h"
#include "runtime_paths.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>
namespace fs = std::filesystem;

static bool screenshot(SDL_Renderer* r, const std::string& name) {
 SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
 if (!s) return false;
 bool ok = SDL_RenderReadPixels(r, nullptr, s->format->format, s->pixels, s->pitch) == 0 && SDL_SaveBMP(s, name.c_str()) == 0;
 SDL_FreeSurface(s);
 return ok;
}

static uint32_t keyboardPad() {
 const Uint8* k = SDL_GetKeyboardState(nullptr);
 uint32_t bits = 0;
 const std::pair<SDL_Scancode, uint32_t> keys[] = {
  {SDL_SCANCODE_A, pad::A}, {SDL_SCANCODE_RETURN, pad::A}, {SDL_SCANCODE_B, pad::B}, {SDL_SCANCODE_ESCAPE, pad::B}, {SDL_SCANCODE_X, pad::X}, {SDL_SCANCODE_Y, pad::Y},
  {SDL_SCANCODE_Q, pad::L}, {SDL_SCANCODE_E, pad::R}, {SDL_SCANCODE_1, pad::ZL}, {SDL_SCANCODE_2, pad::ZR}, {SDL_SCANCODE_EQUALS, pad::Plus}, {SDL_SCANCODE_MINUS, pad::Minus},
  {SDL_SCANCODE_UP, pad::Up}, {SDL_SCANCODE_DOWN, pad::Down}, {SDL_SCANCODE_LEFT, pad::Left}, {SDL_SCANCODE_RIGHT, pad::Right}, {SDL_SCANCODE_3, pad::LStick}, {SDL_SCANCODE_4, pad::RStick}};
 for (auto& key : keys) if (k[key.first]) bits |= key.second;
 return bits;
}

// A small generated top-down scene that stands in for a GBA game picture.
static std::vector<Uint32> fakeFrame(int seed) {
 const int W = 240, H = 160;
 std::vector<Uint32> px(size_t(W) * H);
 unsigned s = unsigned(seed) * 9301u + 49297u;
 auto rnd = [&] { s = (s * 9301u + 49297u) % 233280u; return s / 233280.f; };
 auto rect = [&](int x, int y, int w, int h, Uint32 c) { for (int j = std::max(0, y); j < std::min(H, y + h); ++j) for (int i = std::max(0, x); i < std::min(W, x + w); ++i) px[size_t(j) * W + i] = 0xFF000000u | c; };
 for (int y = 0; y < 10; ++y) for (int x = 0; x < 15; ++x) { rect(x * 16, y * 16, 16, 16, rnd() < .15f ? 0x70C870 : 0x60B860); if (rnd() < .25f) rect(x * 16 + int(rnd() * 12), y * 16 + int(rnd() * 12), 2, 2, 0x88D888); }
 int path = int(rnd() * 4) + 5;
 rect(path * 16, 0, 32, 160, 0xE0C890); rect(0, 96, 240, 32, 0xE0C890);
 rect(176, 0, 64, 48, 0x4890E0);
 for (int i = 0; i < 9; ++i) { int x = int(rnd() * 15), y = int(rnd() * 10); if ((x >= path && x <= path + 1) || y == 6 || y == 7 || (x >= 11 && y < 3)) continue; rect(x * 16 + 2, y * 16, 12, 12, 0x2F7A3A); rect(x * 16 + 2, y * 16 + 8, 12, 4, 0x205A28); rect(x * 16 + 6, y * 16 + 12, 4, 4, 0x704020); }
 rect(32, 16, 64, 48, 0xD8D8E8); rect(28, 10, 72, 14, 0xD04848); rect(56, 48, 10, 16, 0x584030);
 int cx = path * 16 + 10, cy = 100;
 rect(cx, cy, 12, 5, 0xE03030); rect(cx + 2, cy + 5, 8, 5, 0xF8D0A8); rect(cx + 1, cy + 10, 10, 5, 0x3050C0);
 rect(4, 128, 232, 28, 0xF8F8F8); rect(4, 128, 232, 2, 0x3868B0); rect(4, 154, 232, 2, 0x3868B0); rect(12, 136, 90, 4, 0x404040); rect(12, 144, 130, 4, 0x404040);
 return px;
}

int main(int argc, char** argv) {
 SDL_SetMainReady();
 if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
 SDL_Window* w = SDL_CreateWindow("AI1 Emu v" AI1EMU_VERSION " - UI preview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
 SDL_Renderer* r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
 if (!r) r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
 if (!r) { SDL_Log("%s", SDL_GetError()); return 2; }
 SDL_RenderSetLogicalSize(r, 1280, 720);
 fs::create_directories("screenshots");
 bool capture = argc > 1 && std::string(argv[1]) == "--capture";
 const auto root = retroverseRoot();
 std::vector<std::string> games = scanLibrary(root);
 // Artwork-only catalogue entries stand in for games when no files are present.
 if (games.empty() && fs::exists(root + "artwork-cache"))
  for (const auto& entry : fs::directory_iterator(root + "artwork-cache"))
   if (entry.path().extension() == ".png") games.push_back(root + "roms/" + entry.path().stem().string() + ".gba");

 previewKeyboard = [](const std::string& guide, const std::string&, std::string& out) {
  if (guide.find("Search") != std::string::npos) out = "pokemon";
  else if (guide.find("name") != std::string::npos) out = "Infinite money";
  else out = "82025BC4 423F";
  return true;
 };

 {
  LibraryUI ui(r);
  ui.setGames(games);
  ui.battery = 72;
  ui.updateButton = "Check for updates";
  std::unique_ptr<GameSession> session;
  std::vector<Uint32> frame = fakeFrame(1);
  SDL_Texture* gameTexture = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 240, 160);
  auto showFrame = [&](int seed) { frame = fakeFrame(seed); SDL_UpdateTexture(gameTexture, nullptr, frame.data(), 240 * 4); if (session) session->captureFrame(frame.data(), 240, 160, 240 * 4, 1); };
  std::vector<char> fakeState(64 * 1024, 7);
  auto startGame = [&](const std::string& path, bool resume) {
   CoreHooks hooks;
   hooks.serializeSize = [&] { return fakeState.size(); };
   hooks.serialize = [&](void* d, size_t n) { std::memcpy(d, fakeState.data(), std::min(n, fakeState.size())); return true; };
   hooks.unserialize = [](const void*, size_t) { return true; };
   hooks.reset = [] {};
   hooks.cheatReset = [] {};
   hooks.cheatSet = [](unsigned, bool, const char*) {};
   SessionOptions opt;
   session = std::make_unique<GameSession>(r, root, path, detectPlatform(path), gameSavePrefix(root, path), ui.coverFor(path), hooks, opt);
   showFrame(1);
   if (resume) session->loadSlot(0, true);
  };
  auto drawGame = [&] {
   SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
   SDL_RenderClear(r);
   SDL_Rect dst = session->gameRect(240, 160, 1.5);
   SDL_SetTextureScaleMode(gameTexture, session->scaleMode());
   SDL_RenderCopy(r, gameTexture, nullptr, &dst);
   session->drawLook(dst, 160);
  };
  auto render = [&] {
   if (session) {
    if (session->menuOpen()) session->drawMenu(drawGame);
    else { drawGame(); session->drawOverlay(); }
   } else ui.draw(false);
  };
  bool running = true;
  int phase = 0;
  unsigned at = 0;
  unsigned frameNo = 0;
  uint32_t lastPad = 0;
  int shot = 0;
  auto save = [&](const char* name) { render(); char file[96]; std::snprintf(file, sizeof file, "screenshots/%02d-%s.bmp", ++shot, name); return screenshot(r, file); };
  while (running) {
   SDL_Event e;
   while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) running = false;
    if (!session) {
     auto cmd = ui.handlePointer(e);
     if (cmd.type == LibraryUI::Command::Launch) startGame(cmd.path, cmd.resume);
     if (cmd.type == LibraryUI::Command::Quit) running = false;
     if (cmd.type == LibraryUI::Command::Search) { std::string q; if (showKeyboard("Search your games", ui.query, q)) ui.setQuery(q); }
    } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.x > 1160 && e.button.y < 120) {
     session->update(0, 0, true);
    }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12) { render(); screenshot(r, "screenshots/manual-" + std::to_string(SDL_GetTicks()) + ".bmp"); }
   }
   uint32_t held = keyboardPad() | (capture && phase >= 17 ? uint32_t(pad::RStick) : 0u), down = held & ~lastPad;
   lastPad = held;
   ui.padHeld = held;
   if (session) {
    if (!session->update(held, down)) { session->finish(true); session.reset(); ui.reloadData(); }
   } else {
    auto cmd = ui.update(down, held);
    if (cmd.type == LibraryUI::Command::Launch) startGame(cmd.path, cmd.resume);
    if (cmd.type == LibraryUI::Command::Quit) running = false;
    if (cmd.type == LibraryUI::Command::Search) { std::string q; if (showKeyboard("Search your games", ui.query, q)) ui.setQuery(q); }
    if (cmd.type == LibraryUI::Command::Update) ui.updateStatus = "Updates are installed from Settings on your Switch.";
   }
   if (capture) {
    // Scripted tour of every v0.5 screen; each capture is a real SDL2 frame.
    auto after = [&](unsigned n) { return frameNo >= at + n; };
    auto next = [&] { ++phase; at = frameNo; };
    auto pressUi = [&](uint32_t bits) { auto cmd = ui.update(bits, 0); if (cmd.type == LibraryUI::Command::Launch) startGame(cmd.path, cmd.resume); };
    switch (phase) {
    case 0: if (after(40)) { if (!save("home")) return 3; pressUi(pad::Down); pressUi(pad::Right); pressUi(pad::Right); next(); } break;
    case 1: if (after(30)) { if (!save("home-shelf")) return 3; ui.tab = LibraryUI::TabLibrary; ui.gridIndex = 1; next(); } break;
    case 2: if (after(40)) { if (!save("library")) return 3; ui.tab = LibraryUI::TabSettings; ui.section = LibraryUI::SecGameplay; ui.sectionFocus = true; ui.settingRow = 0; next(); } break;
    case 3: if (after(25)) { if (!save("settings-gameplay")) return 3; ui.section = LibraryUI::SecThemes; ui.settingRow = 0; next(); } break;
    case 4: if (after(25)) { if (!save("settings-themes")) return 3; ui.section = LibraryUI::SecControls; ui.padHeld = pad::A | pad::L; next(); } break;
    case 5: if (after(25)) { if (!save("settings-controls")) return 3; ui.section = LibraryUI::SecUpdates; ui.sectionFocus = false; next(); } break;
    case 6: if (after(25)) { if (!save("settings-updates")) return 3; ui.section = LibraryUI::SecMap; next(); } break;
    case 7: if (after(30) && ui.mapState == 2) { if (!save("settings-map")) return 3; ui.section = LibraryUI::SecAbout; next(); } break;
    case 8: if (after(30) && (ui.storageState == 2 || after(400))) { if (!save("settings-about")) return 3; ui.settings.theme = "black"; ui.reloadData(); ui.settings.theme = "black"; ui.paint.setTheme(ui::presetTheme("black")); ui.tab = LibraryUI::TabHome; ui.homeZone = 0; next(); } break;
    case 9: if (after(25)) { if (!save("home-black")) return 3; ui.paint.setTheme(resolveTheme(loadSettings(root))); ui.tab = LibraryUI::TabHome; ui.homeZone = 0; ui.homeIndex = 0; pressUi(pad::A); next(); } break;
    case 10: if (after(20) && session) { showFrame(3); session->saveSlot(1, true); showFrame(4); session->saveSlot(2, true); showFrame(1); session->update(pad::Plus | pad::Minus, pad::Plus | pad::Minus); next(); } break;
    case 11: if (after(20)) { if (!save("menu-resume")) return 3; session->update(0, pad::Down); session->update(0, pad::A); next(); } break;
    case 12: if (after(20)) { if (!save("menu-save")) return 3; for (uint32_t b : {pad::B, pad::Down, pad::Down, pad::Down, pad::A}) session->update(0, b); next(); } break;
    case 13: if (after(20)) { if (!save("menu-speed")) return 3; for (uint32_t b : {pad::B, pad::Down, pad::A, pad::X, pad::Up}) session->update(0, b); next(); } break;
    case 14: if (after(20)) { if (!save("menu-cheats")) return 3; for (uint32_t b : {pad::B, pad::Down, pad::A}) session->update(0, b); next(); } break;
    case 15: if (after(20)) { if (!save("menu-display")) return 3; for (uint32_t b : {pad::B, pad::Up, pad::Up, pad::Up, pad::Up, pad::A, pad::Right}) session->update(0, b); next(); } break;
    case 16: if (after(20)) { if (!save("menu-load")) return 3; session->update(0, pad::B); session->update(0, pad::B); session->toast("Saved to Slot 2"); next(); } break;
    case 17: if (after(20)) { if (!save("game-overlay")) return 3; running = false; } break;
    }
   }
   if (session && !session->menuOpen()) for (int i = session->framesThisLoop(); i > 0; --i) session->afterFrame();
   render();
   SDL_RenderPresent(r);
   ++frameNo;
   if (capture && frameNo > 3000) running = false;
  }
  if (session) session->finish(false);
  SDL_DestroyTexture(gameTexture);
 }
 SDL_DestroyRenderer(r);
 SDL_DestroyWindow(w);
 SDL_Quit();
 return 0;
}
