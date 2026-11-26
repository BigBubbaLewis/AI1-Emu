// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Switch frontend: the library, plus GBA / GB games, which run in this process
// through mGBA. Other consoles are separate core NROs chainloaded with
// "<core> <game> <data root> <platform> <play|resume> <cover>".
#include <switch.h>
#include <SDL.h>
#include "library_ui.h"
#include "game_session.h"
#include "keyboard.h"
#include "safe_file.h"
#include "runtime_paths.h"
#include "online_updater.h"
#include "game_database.h"
#include "platform_library.h"
#include "legacy_migration.h"
#include "core_status.h"
#include "cpu_boost.h"
#include "cover_fetch.h"
#include "app_log.h"
#include "sysclk_profile.h"
#include "app_data.h"
#include <cstdio>
#include <libretro.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
namespace fs = std::filesystem;

static const char* sysclkConfig = "sdmc:/config/sys-clk/config.ini";
static bool sysclkInstalled() {
 std::error_code ec;
 return fs::exists("sdmc:/atmosphere/contents/00FF0000636C6BFF/exefs.nsp", ec) || fs::exists("sdmc:/config/sys-clk", ec);
}
// This process's title ID, which sys-clk uses to pick a profile (the forwarder's, or the
// game's when opened through the Homebrew Menu in application mode).
static std::string ownTitleId() {
 u64 id = 0;
 if (R_FAILED(svcGetInfo(&id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0))) return "0000000000000000";
 char hex[17];
 std::snprintf(hex, sizeof hex, "%016lX", (unsigned long)id);
 return hex;
}
static SDL_Renderer* renderer;
static SDL_Texture* frame;
static SDL_AudioDeviceID audio;
static PadState padState;
static uint64_t held;
static unsigned pixel = RETRO_PIXEL_FORMAT_0RGB1555, frameW = 0, frameH = 0;
static std::string root, systemDirectory, coreSaveDirectory;
static double displayAspect = 1.5;
static GameSession* session = nullptr;
static bool suppressPresent = false;
static std::vector<int16_t> audioScratch;
static CpuBoost boost;

static uint32_t padBits(uint64_t buttons) {
 static const std::pair<uint64_t, uint32_t> keys[] = {
  {HidNpadButton_A, pad::A}, {HidNpadButton_B, pad::B}, {HidNpadButton_X, pad::X}, {HidNpadButton_Y, pad::Y}, {HidNpadButton_L, pad::L}, {HidNpadButton_R, pad::R},
  {HidNpadButton_ZL, pad::ZL}, {HidNpadButton_ZR, pad::ZR}, {HidNpadButton_Plus, pad::Plus}, {HidNpadButton_Minus, pad::Minus}, {HidNpadButton_Up, pad::Up},
  {HidNpadButton_Down, pad::Down}, {HidNpadButton_Left, pad::Left}, {HidNpadButton_Right, pad::Right}, {HidNpadButton_StickL, pad::LStick}, {HidNpadButton_StickR, pad::RStick}};
 uint32_t bits = 0;
 for (auto& k : keys) if (buttons & k.first) bits |= k.second;
 return bits;
}

static bool environment(unsigned cmd, void* data) {
 switch (cmd) {
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: pixel = *(unsigned*)data; return pixel == RETRO_PIXEL_FORMAT_RGB565 || pixel == RETRO_PIXEL_FORMAT_XRGB8888 || pixel == RETRO_PIXEL_FORMAT_0RGB1555;
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char**)data = systemDirectory.c_str(); return true;
 case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data = coreSaveDirectory.c_str(); return true;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)data = true; return true;
 default: return false;
 }
}

static void drawGameFrame() {
 SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
 SDL_RenderClear(renderer);
 if (!frame || !session) return;
 SDL_Rect dst = session->gameRect(int(frameW), int(frameH), displayAspect);
 SDL_SetTextureScaleMode(frame, session->scaleMode());
 SDL_RenderCopy(renderer, frame, nullptr, &dst);
 session->drawLook(dst, int(frameH));
}

static void video(const void* data, unsigned w, unsigned h, size_t pitch) {
 if (!data) return;
 static unsigned lastPixel = 99;
 if (w != frameW || h != frameH || pixel != lastPixel) {
  if (frame) SDL_DestroyTexture(frame);
  frame = SDL_CreateTexture(renderer, pixel == RETRO_PIXEL_FORMAT_RGB565 ? SDL_PIXELFORMAT_RGB565 : pixel == RETRO_PIXEL_FORMAT_XRGB8888 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, w, h);
  frameW = w; frameH = h; lastPixel = pixel;
 }
 if (!frame) return;
 SDL_UpdateTexture(frame, nullptr, data, int(pitch));
 if (session) session->captureFrame(data, w, h, pitch, pixel);
 if (suppressPresent) return;
 drawGameFrame();
 if (session) session->drawOverlay();
 SDL_RenderPresent(renderer);
}

static size_t samples(const int16_t* data, size_t n) {
 float gain = session ? session->audioScale() : 1.f;
 if (audio && gain > 0) {
  if (SDL_GetQueuedAudioSize(audio) > 48000) SDL_ClearQueuedAudio(audio);
  if (gain < .999f) {
   audioScratch.assign(data, data + n * 2);
   for (auto& s : audioScratch) s = int16_t(s * gain);
   SDL_QueueAudio(audio, audioScratch.data(), Uint32(n * 4));
  } else SDL_QueueAudio(audio, data, Uint32(n * 4));
 }
 return n;
}
static void sample(int16_t l, int16_t r) { int16_t v[] = {l, r}; samples(v, 1); }
static int16_t input(unsigned port, unsigned device, unsigned, unsigned id) {
 if (port || device != RETRO_DEVICE_JOYPAD || (session && session->menuOpen())) return 0;
 static const uint64_t keys[] = {HidNpadButton_B, HidNpadButton_Y, HidNpadButton_Minus, HidNpadButton_Plus, HidNpadButton_Up, HidNpadButton_Down, HidNpadButton_Left, HidNpadButton_Right, HidNpadButton_A, HidNpadButton_X, HidNpadButton_L, HidNpadButton_R};
 return id < 12 && (held & keys[id]) ? 1 : 0;
}
static std::vector<char> read(const std::string& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
// Writes to <p>.tmp, then swaps it in (keeping the old file as <p>.previous). A plain rename
// onto an existing file fails on the Switch, which left every save after the first in .tmp.
static bool write(const std::string& p, const void* d, size_t n) { return safeWriteFile(p, d, n); }

int main() {
 { std::error_code ec; fs::create_directories("sdmc:/switch/ai1emu/logs", ec); }
 appLogStart("AI1 Emu " AI1EMU_VERSION " starting (applet type " + std::to_string(int(appletGetAppletType())) + ")");
 OnlineUpdater updater;
 bool installUpdate = false;
 if (R_FAILED(romfsInit())) { appLog("RomFS failed"); return 3; }
 padConfigureInput(1, HidNpadStyleSet_NpadStandard);
 padInitializeDefault(&padState);
 hidInitializeTouchScreen();
 bool power = R_SUCCEEDED(psmInitialize());
 if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return 1;
 SDL_Window* window = SDL_CreateWindow("AI1 Emu", 0, 0, 1280, 720, 0);
 renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
 if (!renderer) { appLog("Renderer failed"); return 2; }
 appLog("Video ready");
 SDL_RenderSetLogicalSize(renderer, 1280, 720);
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
 Uint32 lastMigrationDraw = 0;
 auto migration = mergeLegacyFolder("sdmc:/switch", [&](const std::string& message) {
  padUpdate(&padState);
  auto buttons = padGetButtons(&padState);
  if ((buttons & (HidNpadButton_Plus | HidNpadButton_Minus)) == (HidNpadButton_Plus | HidNpadButton_Minus)) return false;
  if (SDL_GetTicks() - lastMigrationDraw > 150) { coreStatus(renderer, message.c_str()); lastMigrationDraw = SDL_GetTicks(); }
  return appletMainLoop();
 });
 appLog("Folder migration done");
 // A 3DS boost profile from the last session ends here: the library and the other
 // emulators run at normal clocks unless their own boost setting is on.
 if (sysclkInstalled() && setSysclkBoost(sysclkConfig, ownTitleId(), false) == SysclkResult::Removed) appLog("3DS boost: sys-clk profile removed");
 root = retroverseRoot();
 systemDirectory = root;
 coreSaveDirectory = root + "saves";
 fs::create_directories(root + "roms");
 fs::create_directories(root + "saves");
 for (const char* dir : {"covers", "screenshots", "metadata", "cheats", "themes"}) fs::create_directories(root + dir);
 for (auto& info : platforms()) { fs::create_directories(root + "roms/" + info.slug); fs::create_directories(root + "saves/" + info.slug); fs::create_directories(root + "system/" + info.slug); fs::create_directories(root + "cheats/" + info.slug); }
 {
  appLog("Creating library");
  LibraryUI ui(renderer);
  appLog("Library created");
  retro_set_environment(environment);
  retro_set_video_refresh(video);
  retro_set_audio_sample(sample);
  retro_set_audio_sample_batch(samples);
  retro_set_input_poll([] {});
  retro_set_input_state(input);
  retro_init();
  retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  appLog("Scanning games");
  ui.setGames(scanLibrary(root));
  appLog("Found " + std::to_string(ui.all.size()) + " games");
  if (!migration.message.empty()) ui.toast(migration.message);
  if (OnlineUpdater::testChannel() && OnlineUpdater::installedCores() != AI1EMU_VERSION)
   ui.toast("Test channel: open Settings > Updates to finish installing the emulators.");
  { std::ifstream errors(root + "launch-error.txt"); std::string line; if (errors && std::getline(errors, line) && !line.empty()) ui.toast(line); }
  std::remove((root + "launch-error.txt").c_str());
  // A game that ends chainloads back here; library-session.txt tells that apart from opening the app.
  const bool returningFromGame = fs::exists(root + "library-session.txt");
  {
   std::ifstream s(root + "library-session.txt");
   std::string tag, path;
   int tab = 0;
   if (s >> tag >> tab && tag == "v5") { s.ignore(); std::getline(s, path); ui.tab = std::clamp(tab, 0, LibraryUI::tabCount - 1); ui.select(path); }
   s.close();
   std::remove((root + "library-session.txt").c_str());
  }

  std::unique_ptr<GameSession> game;
  std::string gamePath, save;
  std::vector<char> rom;
  bool checkedUpdates = false;
  // Automatic update checks: every time AI1 Emu is opened, and every 5 hours while it
  // runs. Coming back from a game only checks once 5 hours have passed. A found update
  // is announced; nothing downloads or installs until the owner presses the button.
  const long long autoCheckEvery = 5 * 60 * 60;
  auto lastAutoCheck = [&] { std::ifstream f(root + "update-check.txt"); long long t = 0; f >> t; return t; };
  auto autoCheckDue = [&] { long long now = std::time(nullptr), last = lastAutoCheck(); return now - last >= autoCheckEvery || now < last; };
  bool autoChecking = false;
  auto autoCheck = [&] {
   auto p = updater.phase();
   if (p == OnlineUpdater::Checking || p == OnlineUpdater::Downloading || p == OnlineUpdater::Available || p == OnlineUpdater::Ready) return;
   updater.check();
   checkedUpdates = autoChecking = true;
   std::ofstream(root + "update-check.txt") << (long long)std::time(nullptr) << "\n";
  };
  // Network work waits until the library has been on screen for about a second, and
  // cover downloads wait until the update check is done, so nothing network-related
  // can hold up the first screen or run alongside the other.
  bool startupCheckPending = !returningFromGame || autoCheckDue();
  unsigned libraryFrames = 0;
  Uint64 lastAutoTick = SDL_GetTicks64();
  CoverFetcher covers;
  bool coversStarted = false;
  OnlineUpdater::Phase loggedPhase = OnlineUpdater::Idle;
  unsigned frames = 0;
  Uint64 lastBattery = 0;
  uint64_t lastHeld = 0;
  auto persist = [&] {
   size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   void* p = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   if (n && p && !write(save + ".sav", p, n)) game->toast("Game save could not be written. Check free SD space.", ui::Icon::Info);
  };
  auto stopGame = [&] {
   if (!game) return;
   game->finish(true);
   boost.set(false);
   persist();
   retro_unload_game();
   session = nullptr;
   game.reset();
   rom.clear();
   if (audio) { SDL_CloseAudioDevice(audio); audio = 0; }
   ui.reloadData();
   ui.select(gamePath);
  };
  auto launch = [&](const std::string& path, bool resume) -> bool {
   auto platform = detectPlatform(path);
   const auto& info = platformInfo(platform);
   if (needsFullMemory(platform) && appletGetAppletType() != AppletType_Application && appletGetAppletType() != AppletType_SystemApplication) {
    ui.toast("This console needs full memory. Open AI1 Emu by holding R while starting a game.");
    return false;
   }
   std::string cover = ui.coverFor(path);
   if (info.core[0]) {
    std::string corePath = "sdmc:/switch/ai1emu/cores/" + std::string(info.core);
    if (!fs::exists(corePath)) { ui.toast("This console's emulator is missing. Install the complete package."); return false; }
    if (!envHasNextLoad()) { ui.toast("This loader can't start emulators. Use Homebrew Menu application mode."); return false; }
    if (path.find_first_of("\"\r\n") != std::string::npos || cover.find_first_of("\"\r\n") != std::string::npos) { ui.toast("Rename this game file to remove quotes."); return false; }
    ui.showLaunching(path);
    appLog("Launch: " + fs::path(path).filename().string() + " with " + info.core);
    std::string args = corePath + " \"" + path + "\" \"" + root + "\" " + std::to_string(int(platform)) + " " + (resume ? "resume" : "play") + " \"" + cover + "\"";
    if (R_SUCCEEDED(envSetNextLoad(corePath.c_str(), args.c_str()))) {
     // Dekopon (3DS) is the upstream emulator and does not report play time back,
     // so record the launch here to keep the game on Home's "Recently played".
     if (platform == Platform::N3DS) {
      recordSession(root, path, (long long)std::time(nullptr), 0);
      // Performance boost for 3DS goes through sys-clk (Dekopon is not ours to clock).
      if (sysclkInstalled() && loadGameplay(root, gameSavePrefix(root, path)).boost) {
       auto r = setSysclkBoost(sysclkConfig, ownTitleId(), true);
       appLog(r == SysclkResult::Applied ? "3DS boost: sys-clk profile applied" : r == SysclkResult::OwnerProfile ? "3DS boost: owner's own sys-clk profile kept" : "3DS boost: sys-clk profile could not be written");
      }
     }
     std::ofstream s(root + "library-session.txt");
     s << "v5 " << ui.tab << "\n" << path;
     return true; // leave the loop; the loader starts the core
    }
    ui.toast("Could not start this console's emulator.");
    return false;
   }
   ui.showLaunching(path);
   appLog("Launch: " + fs::path(path).filename().string() + " in-process");
   ui.stopStorageScan();
   systemDirectory = root + "system/" + info.slug;
   coreSaveDirectory = root + "saves/" + info.slug;
   if (platform == Platform::GBA && fs::exists(root + "gba_bios.bin")) systemDirectory = root;
   rom = read(path);
   appLog("Launch: ROM read, " + std::to_string(rom.size() / 1024) + " KB");
   retro_game_info gi{};
   gi.path = path.c_str();
   gi.data = rom.data();
   gi.size = rom.size();
   if (!retro_load_game(&gi)) { ui.toast("This game could not be loaded."); return false; }
   appLog("Launch: game loaded");
   gamePath = path;
   save = gameSavePrefix(root, path);
   fs::create_directories(fs::path(save).parent_path());
   auto ram = read(newestSave(save + ".sav"));
   auto n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   if (!ram.empty() && ram.size() <= n && retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)) memcpy(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM), ram.data(), ram.size());
   retro_system_av_info av{};
   retro_get_system_av_info(&av);
   displayAspect = av.geometry.aspect_ratio > 0 ? av.geometry.aspect_ratio : double(av.geometry.base_width) / av.geometry.base_height;
   SDL_AudioSpec spec{};
   spec.freq = int(av.timing.sample_rate);
   spec.format = AUDIO_S16SYS;
   spec.channels = 2;
   spec.samples = 1024;
   audio = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
   if (audio) SDL_PauseAudioDevice(audio, 0);
   CoreHooks hooks;
   hooks.serializeSize = [] { return retro_serialize_size(); };
   hooks.serialize = [](void* d, size_t s) { return retro_serialize(d, s); };
   hooks.unserialize = [](const void* d, size_t s) { return retro_unserialize(d, s); };
   hooks.reset = [] { retro_reset(); };
   hooks.cheatReset = [] { retro_cheat_reset(); };
   hooks.cheatSet = [](unsigned i, bool on, const char* code) { retro_cheat_set(i, on, code); };
   SessionOptions opt;
   opt.setBoost = [](bool on) { boost.set(on); };
   game = std::make_unique<GameSession>(renderer, root, path, platform, save, cover, hooks, opt);
   session = game.get();
   if (resume) game->loadSlot(0, true);
   auto stamp = std::to_string(std::time(nullptr));
   write(save + ".lastplayed", stamp.data(), stamp.size());
   frames = 0;
   return false;
  };

  while (appletMainLoop()) {
   padUpdate(&padState);
   held = padGetButtons(&padState);
   uint64_t downRaw = held & ~lastHeld;
   lastHeld = held;
   uint32_t heldBits = padBits(held), downBits = padBits(downRaw);
   auto stick = padGetStickPos(&padState, 0);
   bool cornerTap = false;
   SDL_Event event;
   while (SDL_PollEvent(&event)) {
    if (game) { if (event.type == SDL_FINGERDOWN && event.tfinger.x > .91f && event.tfinger.y < .17f) cornerTap = true; continue; }
    auto cmd = ui.handlePointer(event);
    if (cmd.type == LibraryUI::Command::Launch) { if (launch(cmd.path, cmd.resume)) goto leave; }
    else if (cmd.type == LibraryUI::Command::Quit) goto leave;
    else if (cmd.type == LibraryUI::Command::Search) { std::string q; if (showKeyboard("Search your games", ui.query, q)) ui.setQuery(q); }
    else if (cmd.type == LibraryUI::Command::Update) {
     if (updater.phase() == OnlineUpdater::Available) updater.download();
     else if (updater.phase() == OnlineUpdater::Ready) { installUpdate = true; goto leave; }
     else updater.check();
    }
   }
   if (startupCheckPending && libraryFrames >= 60) { startupCheckPending = false; appLog("Starting automatic update check"); autoCheck(); }
   if (libraryFrames >= 60 && SDL_GetTicks64() - lastAutoTick > 60000) { lastAutoTick = SDL_GetTicks64(); if (autoCheckDue()) { appLog("Starting 5-hour update check"); autoCheck(); } }
   if (updater.phase() != loggedPhase) { loggedPhase = updater.phase(); appLog("Updater: " + updater.label() + " - " + updater.message()); }
   if (autoChecking && updater.phase() != OnlineUpdater::Checking) {
    autoChecking = false;
    if (updater.phase() == OnlineUpdater::Available) {
     const char* note = "Update available. Open Settings > Updates to download it.";
     if (game) game->toast(note, ui::Icon::Update); else ui.toast(note);
    }
   }
   if (game) {
    boost.keep();
    if (!game->update(heldBits, downBits, cornerTap)) { stopGame(); continue; }
    if (game->menuOpen()) {
     if (audio) SDL_ClearQueuedAudio(audio);
     game->drawMenu(drawGameFrame);
     SDL_RenderPresent(renderer);
     continue;
    }
    int n = game->framesThisLoop();
    for (int i = 0; i < n; ++i) {
     suppressPresent = i + 1 < n;
     retro_run();
     game->afterFrame();
     if (++frames % 300 == 0) persist();
    }
    suppressPresent = false;
    continue;
   }

   // Library
   if (stick.x > 16000) heldBits |= pad::Right;
   if (stick.x < -16000) heldBits |= pad::Left;
   if (stick.y > 16000) heldBits |= pad::Up;
   if (stick.y < -16000) heldBits |= pad::Down;
   ui.padHeld = padBits(held);
   auto left = padGetStickPos(&padState, 0), right = padGetStickPos(&padState, 1);
   ui.stickLX = left.x / 32767.f; ui.stickLY = -left.y / 32767.f; ui.stickRX = right.x / 32767.f; ui.stickRY = -right.y / 32767.f;
   if (power && SDL_GetTicks64() - lastBattery > 30000) {
    lastBattery = SDL_GetTicks64();
    u32 percent = 0;
    if (R_SUCCEEDED(psmGetBatteryChargePercentage(&percent))) ui.battery = int(percent);
    PsmChargerType charger;
    if (R_SUCCEEDED(psmGetChargerType(&charger))) ui.charging = charger != PsmChargerType_Unconnected;
   }
   auto cmd = ui.update(downBits, heldBits);
   if (cmd.type == LibraryUI::Command::Launch) { if (launch(cmd.path, cmd.resume)) break; }
   else if (cmd.type == LibraryUI::Command::Quit) break;
   else if (cmd.type == LibraryUI::Command::Search) { std::string q; if (showKeyboard("Search your games", ui.query, q)) ui.setQuery(q); }
   else if (cmd.type == LibraryUI::Command::Update) {
    if (cmd.path == "check") { if (!checkedUpdates) { updater.check(); checkedUpdates = true; } }
    else if (updater.phase() == OnlineUpdater::Available) updater.download();
    else if (updater.phase() == OnlineUpdater::Ready) { installUpdate = true; break; }
    else updater.check();
   }
   // Covers missing from the SD card download in the background and appear as they land.
   if (!coversStarted && libraryFrames >= 60 && !startupCheckPending && updater.phase() != OnlineUpdater::Checking) { coversStarted = true; appLog("Looking for missing covers"); }
   if (coversStarted && !covers.offline()) for (auto& job : ui.missingCovers(4)) covers.add({job.first, job.second});
   for (auto& done : covers.takeFinished()) ui.coverArrived(done);
   ui.updateButton = updater.label();
   ui.updateStatus = updater.message();
   ui.updateBusy = updater.phase() == OnlineUpdater::Checking || updater.phase() == OnlineUpdater::Downloading;
   ui.draw();
   if (libraryFrames < 60 && ++libraryFrames == 1) appLog("First library frame drawn");
  }
 leave:
  appLog("Leaving the library");
  if (game) stopGame();
  ui.stopStorageScan();
  retro_deinit();
  if (audio) SDL_CloseAudioDevice(audio);
  if (frame) SDL_DestroyTexture(frame);
 }
 if (power) psmExit();
 SDL_DestroyRenderer(renderer);
 SDL_DestroyWindow(window);
 SDL_Quit();
 romfsExit();
 appLog("Shut down; the next program starts now");
 if (installUpdate && !updater.apply()) {
  std::ofstream error("sdmc:/switch/ai1emu/update-error.txt");
  error << "Update could not be installed; the previous app was retained. Retry from Settings.";
 }
 return 0;
}
