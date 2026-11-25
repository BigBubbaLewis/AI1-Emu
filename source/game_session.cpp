// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
#include "game_session.h"
#include "safe_file.h"
#include "keyboard.h"
#include "third_party/stb_image.h"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
using ui::Icon;

namespace {
enum Item { Resume, SaveState, LoadState, Rewind, Speed, Cheats, Display, Controls, Screenshot, Restart, Quit, ItemCount };
const char* itemName(int i) {
 static const char* names[ItemCount] = {"Resume", "Save state", "Load state", "Rewind", "Speed", "Cheats", "Display", "Controls", "Screenshot", "Restart", "Save and quit"};
 return names[i];
}
Icon itemIcon(int i) {
 static const Icon icons[ItemCount] = {Icon::Play, Icon::Save, Icon::Load, Icon::Rewind, Icon::FastForward, Icon::Cheat, Icon::Display, Icon::Pad, Icon::Camera, Icon::Restart, Icon::Power};
 return icons[i];
}
enum Dialog { NoDialog, OverwriteSlot, DeleteSlot, RestartGame, DeleteCheat };
std::string shortTitle(const std::string& path) { return ui::prettyTitle(fs::path(path).stem().string()); }
std::vector<char> readFile(const std::string& p) {
 std::ifstream f(p, std::ios::binary | std::ios::ate);
 if (!f) f.open(p+".previous", std::ios::binary | std::ios::ate);
 if (!f) return {};
 auto n = f.tellg();
 if (n <= 0 || n > 256LL * 1024 * 1024) return {};
 std::vector<char> out(static_cast<size_t>(n));
 f.seekg(0);
 f.read(out.data(), out.size());
 return f ? out : std::vector<char>{};
}
bool writeFile(const std::string& p, const void* d, size_t n) { return safeWriteFile(p,d,n); }
std::string slotName(int slot) { return slot == 0 ? "Auto save" : "Slot " + std::to_string(slot); }
std::string clockText(long long stamp) {
 if (stamp <= 0) return "";
 std::time_t t = std::time_t(stamp);
 char b[32];
 std::strftime(b, sizeof b, "%I:%M %p", std::localtime(&t));
 std::string s = b;
 if (s[0] == '0') s.erase(0, 1);
 return formatWhen(stamp, std::time(nullptr)) + " " + s;
}
} // namespace

GameSession::GameSession(SDL_Renderer* renderer, std::string rootDir, std::string gamePath, Platform p, std::string savePrefix,
                         std::string coverPath, CoreHooks h, SessionOptions o)
 : r(renderer), root(std::move(rootDir)), game(std::move(gamePath)), prefix(std::move(savePrefix)), coverFile(std::move(coverPath)),
   platform(p), hooks(std::move(h)), opt(std::move(o)) {
 paint = std::make_unique<ui::Painter>(r);
 app = loadSettings(root);
 paint->setTheme(resolveTheme(app));
 paint->family = app.font;
 paint->corners = app.corners;
 perGame = hasGameOverrides(prefix);
 settings = loadGameplay(root, prefix);
 if (settings.boost && opt.setBoost) opt.setBoost(true);
 title = shortTitle(game);
 cheatFile = cheatPath(root, game);
 cheats = loadCheats(cheatFile);
 if (!cheats.empty()) applyCheats();
 started = (long long)std::time(nullptr);
 lastTick = SDL_GetTicks64();
 if (!app.tipShown) {
  tipUntil = lastTick + 7000;
  app.tipShown = true;
  ::saveSettings(root, app);
 }
}

GameSession::~GameSession() {
 if (cover.t) SDL_DestroyTexture(cover.t);
 for (auto& s : slotPics) if (s.t) SDL_DestroyTexture(s.t);
}

// ------------------------------------------------------------------ input
bool GameSession::update(uint32_t held, uint32_t down, bool cornerTap) {
 pollCheats();
 Uint64 now = SDL_GetTicks64();
 if (!open) playMs += std::min<Uint64>(now - lastTick, 250);
 lastTick = now;
 const uint32_t chord = pad::Plus | pad::Minus;
 if (((held & chord) == chord && (down & chord)) || cornerTap) {
  if (open) closeMenu(); else openMenu();
  return !quitRequested;
 }
 if (open) { handleMenu(down); return !quitRequested; }

 // Quick save / load: ZR / ZL (N64: hold Minus as well, because ZL is the Z trigger).
 bool quickAllowed = !opt.n64 || (held & pad::Minus);
 if (quickAllowed && (down & pad::ZR)) saveSlot(1);
 if (quickAllowed && (down & pad::ZL)) loadSlot(1);

 if (opt.hotkeys) {
  if (settings.ffToggle) { if (down & pad::RStick) { ffActive = !ffActive; } }
  else ffActive = (held & pad::RStick) != 0 || ffHeldPrev;
  bool canRewind = opt.rewind && settings.rewind;
  rewindActive = false;
  if (canRewind && (held & pad::LStick)) rewindActive = stepRewind();
 }
 return true;
}

int GameSession::framesThisLoop() {
 if (open) return 0;
 if (rewindActive) return 1;
 if (!ffActive) { ffAccumulator = 0; return 1; }
 ffAccumulator += fastForwardSpeeds()[settings.ffSpeed];
 int n = int(ffAccumulator);
 ffAccumulator -= n;
 return std::max(1, n);
}

void GameSession::afterFrame() {
 ++frameCounter;
 if (!rewindActive && opt.rewind && settings.rewind && frameCounter % rewindInterval == 0) captureRewind();
}

void GameSession::openMenu() {
 open = true;
 paneFocus = false;
 item = Resume;
 row = 0;
 confirm = NoDialog;
 browsingFiles = false;
 ffActive = false;
 ffHeldPrev = false;
 rewindActive = false;
 app = loadSettings(root);
 paint->setTheme(resolveTheme(app));
 paint->family = app.font;
 paint->corners = app.corners;
 for (auto& s : slotPics) s.stamp = -1;
}

void GameSession::closeMenu() {
 open = false;
 confirm = NoDialog;
 if (settingsDirty) { saveSettings(); settingsDirty = false; }
}

int GameSession::rowCount() const {
 switch (item) {
 case SaveState: case LoadState: return opt.states ? slotCount : 0;
 case Rewind: return opt.rewind ? 1 : 0;
 case Speed: return 7;
 case Cheats: return !opt.cheats ? 0 : browsingOnline ? int(onlineChoices.size()) : browsingFiles ? int(files.size()) : int(cheats.size()) + 3;
 case Display: return opt.dualScreen ? 4 : 3;
 default: return 0;
 }
}

void GameSession::handleMenu(uint32_t down) {
 if (confirm != NoDialog) {
  if (down & pad::A) {
   int c = confirm;
   confirm = NoDialog;
   if (c == OverwriteSlot) saveSlot(confirmSlot);
   else if (c == DeleteSlot) { std::remove(slotStatePath(prefix, confirmSlot).c_str()); std::remove(slotThumbPath(prefix, confirmSlot).c_str()); slotPics[confirmSlot].stamp = -1; toast(slotName(confirmSlot) + " deleted", Icon::Trash); }
   else if (c == RestartGame) { if (hooks.reset) hooks.reset(); rewindBuffer.clear(); rewindBytes = 0; closeMenu(); toast("Game restarted", Icon::Restart); }
   else if (c == DeleteCheat && confirmSlot < int(cheats.size())) { cheats.erase(cheats.begin() + confirmSlot); saveCheats(cheatFile, cheats); applyCheats(); row = std::min(row, rowCount() - 1); }
  } else if (down & pad::B) confirm = NoDialog;
  return;
 }
 if (!paneFocus) {
  if (down & pad::Up) item = (item + ItemCount - 1) % ItemCount;
  if (down & pad::Down) item = (item + 1) % ItemCount;
  if (down & pad::B) { closeMenu(); return; }
  if (down & (pad::A | pad::Right)) {
   if (item == Resume) { if (down & pad::A) closeMenu(); return; }
   if (item == Screenshot) { if (down & pad::A) takeScreenshot(); return; }
   if (item == Restart) { if (down & pad::A) confirm = RestartGame; return; }
   if (item == Quit) { if (down & pad::A) { quitRequested = true; closeMenu(); } return; }
   if (rowCount() > 0) { paneFocus = true; row = item == SaveState ? 1 : 0; }
  }
  return;
 }
 int rows = rowCount();
 bool grid = item == SaveState || item == LoadState;
 if (down & pad::B) { if (browsingOnline) { browsingOnline=false; row=int(cheats.size())+2; return; } if (browsingFiles) { browsingFiles = false; row = int(cheats.size()) + 1; } else paneFocus = false; return; }
 if (grid) {
  int minCell = item == SaveState ? 1 : 0;
  if (down & pad::Left) { if (row % 3 == 0 || row - 1 < minCell) paneFocus = false; else --row; }
  if ((down & pad::Right) && row % 3 < 2) ++row;
  if ((down & pad::Up) && row >= 3 && row - 3 >= minCell) row -= 3;
  if ((down & pad::Down) && row < 3) row += 3;
  if (down & pad::A) activate();
  if ((down & pad::Y) && row >= 1 && slotTime(prefix, row)) { confirm = DeleteSlot; confirmSlot = row; }
  if ((down & pad::X) && item == LoadState && !undoState.empty()) {
   if (hooks.unserialize(undoState.data(), undoState.size())) { toast("Load undone", Icon::Undo); undoState.clear(); rewindBuffer.clear(); rewindBytes = 0; }
  }
  return;
 }
 if (rows <= 0) { paneFocus = false; return; }
 if (down & pad::Up) row = (row + rows - 1) % rows;
 if (down & pad::Down) row = (row + 1) % rows;
 bool adjustable = item == Speed || item == Display || item == Rewind;
 if (down & pad::Left) { if (adjustable) adjust(-1); else paneFocus = false; }
 if ((down & pad::Right) && adjustable) adjust(1);
 if (down & pad::A) activate();
 if (item == Cheats && !browsingFiles && !browsingOnline) {
  if (down & pad::X) { row = int(cheats.size()); activate(); }
  if ((down & pad::Y) && row < int(cheats.size())) { confirm = DeleteCheat; confirmSlot = row; }
 }
}

void GameSession::adjust(int d) {
 auto cycle = [d](int v, int n) { return (v + d + n) % n; };
 if (item == Rewind) { settings.rewind = !settings.rewind; settingsDirty = true; if (!settings.rewind) { rewindBuffer.clear(); rewindBytes = 0; } return; }
 if (row == 0 && (item == Speed || item == Display)) {
  perGame = !perGame;
  if (!perGame) { clearGameOverrides(prefix); settings = loadSettings(root).gameplay; if (opt.setBoost) opt.setBoost(settings.boost); }
  settingsDirty = true;
  return;
 }
 if (item == Speed) {
  switch (row) {
  case 1: settings.ffSpeed = cycle(settings.ffSpeed, 4); break;
  case 2: settings.ffToggle = !settings.ffToggle; break;
  case 3: ffHeldPrev = !ffHeldPrev; ffActive = ffHeldPrev; return;
  case 4: settings.muteFastForward = !settings.muteFastForward; break;
  case 5: settings.volume = cycle(settings.volume, 4); break;
  case 6: settings.boost = !settings.boost; if (opt.setBoost) opt.setBoost(settings.boost); break;
  }
  settingsDirty = true;
 } else if (item == Display) {
  if (row == 1) {
   if (opt.dualScreen && opt.dsLayout) { *opt.dsLayout = cycle(*opt.dsLayout, 3); return; }
   if (!opt.screenModes) return;
   settings.screen = cycle(settings.screen, opt.n64 ? 2 : 3);
  }
  if (row == 2) { if (!opt.looks) return; settings.look = cycle(settings.look, 4); }
  if (row == 3 && opt.dualScreen) { settings.dsThreaded = !settings.dsThreaded; toast("Takes effect the next time you start this game", Icon::Info); }
  settingsDirty = true;
 }
}

void GameSession::activate() {
 switch (item) {
 case SaveState:
  if (row < 1) return;
  if (slotTime(prefix, row)) { confirm = OverwriteSlot; confirmSlot = row; } else saveSlot(row);
  break;
 case LoadState:
  if (!slotTime(prefix, row)) { toast(slotName(row) + " is empty", Icon::Info); return; }
  if (loadSlot(row)) closeMenu();
  break;
 case Speed: case Display: case Rewind: adjust(1); break;
 case Cheats:
  if (browsingOnline) {
   if(row>=0 && row<int(onlineChoices.size())) {
    if(onlineCheats.phase()==OnlineCheats::Downloading) return;
    onlineCheats.download(onlineChoices[row],root+"cheats/cache/");
    browsingOnline=false; row=int(cheats.size())+2;
   }
  } else if (browsingFiles) {
   if (row < int(files.size())) {
    auto extra = loadCheats(files[row]);
    for (auto& c : extra) { c.enabled = false; cheats.push_back(c); }
    saveCheats(cheatFile, cheats);
    applyCheats();
    toast(std::to_string(extra.size()) + (extra.size() == 1 ? " cheat added" : " cheats added"), Icon::Cheat);
    browsingFiles = false;
    row = 0;
   }
  } else if (row < int(cheats.size())) {
   cheats[row].enabled = !cheats[row].enabled;
   saveCheats(cheatFile, cheats);
   applyCheats();
  } else if (row == int(cheats.size())) {
   std::string name, code;
   if (!showKeyboard("Cheat name", "", name, 48)) { toast("Keyboard unavailable. Put a .cht file in cheats/" + std::string(platformInfo(platform).slug) + "/", Icon::Info); return; }
   if (!showKeyboard("Code (separate lines with +)", "", code, 200) || code.empty()) return;
   cheats.push_back({name.empty() ? "Cheat " + std::to_string(cheats.size() + 1) : name, code, true});
   saveCheats(cheatFile, cheats);
   applyCheats();
   toast("Cheat added and turned on", Icon::Cheat);
  } else if(row == int(cheats.size())+2) {
   if(onlineCheats.phase()==OnlineCheats::Searching || onlineCheats.phase()==OnlineCheats::Downloading) {toast(onlineCheats.message(),Icon::Info);return;}
   onlineCheats.search(platformInfo(platform).slug,fs::path(game).filename().string());
  } else {
   files = cheatFilesFor(root, game);
   if (files.empty()) { toast("No .cht files in cheats/" + std::string(platformInfo(platform).slug) + "/", Icon::Folder); return; }
   browsingFiles = true;
   row = 0;
  }
  break;
 default: break;
 }
}

void GameSession::saveSettings() {
 if (perGame) saveGameOverrides(prefix, settings);
 else {
  clearGameOverrides(prefix);
  app = loadSettings(root);
  app.gameplay = settings;
  ::saveSettings(root, app);
 }
}

void GameSession::applyCheats() {
 if (!opt.cheats || !hooks.cheatReset || !hooks.cheatSet) return;
 hooks.cheatReset();
 for (size_t i = 0; i < cheats.size(); ++i) if (cheats[i].enabled) hooks.cheatSet(unsigned(i), true, cheats[i].code.c_str());
}

void GameSession::pollCheats() {
 auto phase=onlineCheats.phase();if(phase==cheatPhase)return;cheatPhase=phase;
 if(phase==OnlineCheats::Choices) {
  onlineChoices=onlineCheats.candidates();
  if(onlineChoices.empty()){toast("No matching database cheats; website search is not available yet",Icon::Info);return;}
  if(open && item==Cheats){browsingOnline=true;browsingFiles=false;paneFocus=true;row=0;}
  else toast("Cheat matches ready. Open Cheats to search again.",Icon::Info);
 } else if(phase==OnlineCheats::Ready) {
  auto downloaded=loadCheats(onlineCheats.cachedFile());auto merged=cheats;size_t added=0;
  for(auto c:downloaded){if(merged.size()>=512)break;if(c.code.size()>8192 || c.code.find_first_of("\r\n\"")!=std::string::npos)continue;if(std::any_of(merged.begin(),merged.end(),[&](const Cheat& old){return old.code==c.code;}))continue;c.enabled=false;merged.push_back(c);++added;}
  if(!added){toast(downloaded.empty()?"Downloaded file has no supported codes":"No new cheats; existing cheats preserved",Icon::Info);return;}
  if(!saveCheats(cheatFile,merged)){toast("Could not save cheats; existing cheats unchanged",Icon::Info);return;}
  cheats=std::move(merged);applyCheats();row=std::min(row,int(cheats.size())+2);toast(std::to_string(added)+" cheats added, all disabled",Icon::Cheat);
 } else if(phase==OnlineCheats::Failed) toast(onlineCheats.message(),Icon::Info);
}

// ------------------------------------------------------------------ states
bool GameSession::saveSlot(int slot, bool silent) {
 if (!opt.states) { if (!silent) toast(opt.statesNote.empty() ? "Save states are not available for this game" : opt.statesNote, Icon::Info); return false; }
 size_t n = hooks.serializeSize ? hooks.serializeSize() : 0;
 if (!n || n > 64u * 1024 * 1024) { if (!silent) toast("Save states are not available for this game", Icon::Info); return false; }
 std::vector<char> state(n);
 std::error_code e;
 fs::create_directories(fs::path(prefix).parent_path(), e);
 if (!hooks.serialize(state.data(), n) || !writeFile(slotStatePath(prefix, slot), state.data(), n)) { if (!silent) toast("Could not save. Check free SD space.", Icon::Info); return false; }
 writeThumb(slotThumbPath(prefix, slot), 320, 240);
 slotPics[slot].stamp = -1;
 if (!silent) toast("Saved to " + slotName(slot), Icon::Check);
 return true;
}

bool GameSession::loadSlot(int slot, bool silent) {
 if (!opt.states) return false;
 auto state = readFile(slotStatePath(prefix, slot));
 if (state.empty()) { if (!silent) toast(slotName(slot) + " is empty", Icon::Info); return false; }
 size_t n = hooks.serializeSize ? hooks.serializeSize() : 0;
 if (n && n <= 64u * 1024 * 1024) { undoState.resize(n); if (!hooks.serialize(undoState.data(), n)) undoState.clear(); }
 if (!hooks.unserialize(state.data(), state.size())) { if (!silent) toast("This save could not be loaded", Icon::Info); return false; }
 rewindBuffer.clear();
 rewindBytes = 0;
 if (!silent) toast("Loaded " + slotName(slot), Icon::Load);
 return true;
}

void GameSession::captureRewind() {
 size_t n = hooks.serializeSize ? hooks.serializeSize() : 0;
 if (!n || n > 32u * 1024 * 1024) return;
 if (rewindScratch.size() < n) rewindScratch.resize(n);
 if (!hooks.serialize(rewindScratch.data(), n)) return;
 uLongf packed = compressBound(uLong(n));
 std::vector<unsigned char> out(packed + 8);
 std::memcpy(out.data(), &n, sizeof(uint32_t));
 if (compress2(out.data() + 8, &packed, rewindScratch.data(), uLong(n), 1) != Z_OK) return;
 out.resize(packed + 8);
 rewindBytes += out.size();
 rewindBuffer.push_back(std::move(out));
 size_t maxSnapshots = size_t(20 * 60 / rewindInterval);
 while (!rewindBuffer.empty() && (rewindBytes > rewindLimit || rewindBuffer.size() > maxSnapshots)) { rewindBytes -= rewindBuffer.front().size(); rewindBuffer.pop_front(); }
}

bool GameSession::stepRewind() {
 if (rewindBuffer.empty()) return false;
 auto snap = std::move(rewindBuffer.back());
 rewindBuffer.pop_back();
 rewindBytes -= snap.size();
 uint32_t n = 0;
 std::memcpy(&n, snap.data(), sizeof n);
 if (rewindScratch.size() < n) rewindScratch.resize(n);
 uLongf len = n;
 if (uncompress(rewindScratch.data(), &len, snap.data() + 8, uLong(snap.size() - 8)) != Z_OK) return false;
 return hooks.unserialize(rewindScratch.data(), len);
}

void GameSession::finish(bool autoSave) {
 if (finished) return;
 finished = true;
 if (autoSave) saveSlot(0, true);
 if (settingsDirty) saveSettings();
 recordSession(root, game, started, (long long)(playMs / 1000));
}

// ------------------------------------------------------------------ pictures
void GameSession::captureFrame(const void* data, unsigned w, unsigned h, size_t pitch, unsigned pixelFormat) {
 if (!data || !w || !h) return;
 size_t bytes = pitch * h;
 if (frameData.size() != bytes) frameData.resize(bytes);
 std::memcpy(frameData.data(), data, bytes);
 frameW = w; frameH = h; framePitch = pitch; framePixel = pixelFormat;
}

bool GameSession::grab(std::vector<Uint32>& argb, int& w, int& h) {
 if (hooks.grabFrame) return hooks.grabFrame(argb, w, h);
 if (frameData.empty()) return false;
 w = int(frameW);
 h = int(frameH);
 if (opt.dualScreen && frameH == frameW * 3 / 2) h /= 2; // top screen only
 argb.resize(size_t(w) * h);
 for (int y = 0; y < h; ++y) {
  const unsigned char* line = frameData.data() + size_t(y) * framePitch;
  for (int x = 0; x < w; ++x) {
   Uint32 c;
   if (framePixel == 1) { Uint32 v; std::memcpy(&v, line + x * 4, 4); c = v | 0xFF000000u; }
   else {
    uint16_t v; std::memcpy(&v, line + x * 2, 2);
    unsigned rr, gg, bb;
    if (framePixel == 2) { rr = (v >> 11) & 31; gg = (v >> 5) & 63; bb = v & 31; gg = (gg << 2) | (gg >> 4); }
    else { rr = (v >> 10) & 31; gg = (v >> 5) & 31; bb = v & 31; gg = (gg << 3) | (gg >> 2); }
    rr = (rr << 3) | (rr >> 2); bb = (bb << 3) | (bb >> 2);
    c = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
   }
   argb[size_t(y) * w + x] = c;
  }
 }
 return true;
}

bool GameSession::writeThumb(const std::string& path, int maxW, int maxH) {
 std::vector<Uint32> px;
 int w = 0, h = 0;
 if (!grab(px, w, h) || w <= 0 || h <= 0) return false;
 float s = std::min(1.f, std::min(float(maxW) / w, float(maxH) / h));
 int tw = std::max(1, int(w * s)), th = std::max(1, int(h * s));
 std::vector<Uint32> out(size_t(tw) * th);
 for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x) out[size_t(y) * tw + x] = px[size_t(y * h / th) * w + size_t(x * w / tw)];
 SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(out.data(), tw, th, 32, tw * 4, SDL_PIXELFORMAT_ARGB8888);
 if (!surface) return false;
 bool ok = SDL_SaveBMP(surface, (path + ".tmp").c_str()) == 0;
 SDL_FreeSurface(surface);
 if (ok) { std::remove(path.c_str()); ok = std::rename((path + ".tmp").c_str(), path.c_str()) == 0; }
 return ok;
}

bool GameSession::takeScreenshot() {
 std::string dir = root + "screenshots/" + platformInfo(platform).slug + "/";
 std::error_code e;
 fs::create_directories(dir, e);
 std::time_t now = std::time(nullptr);
 char stamp[32];
 std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
 bool ok = writeThumb(dir + fs::path(game).stem().string() + "-" + stamp + ".bmp", 4096, 4096);
 toast(ok ? "Screenshot saved" : "Could not save the screenshot", ok ? Icon::Camera : Icon::Info);
 return ok;
}

GameSession::Pic& GameSession::slotPicture(int slot) {
 Pic& p = slotPics[slot];
 long long t = slotTime(prefix, slot);
 if (p.stamp == t) return p;
 if (p.t) SDL_DestroyTexture(p.t);
 p = {};
 p.stamp = t;
 if (!t) return p;
 if (SDL_Surface* s = SDL_LoadBMP(slotThumbPath(prefix, slot).c_str())) {
  p.t = SDL_CreateTextureFromSurface(r, s);
  p.w = s->w; p.h = s->h;
  SDL_FreeSurface(s);
 }
 return p;
}

void GameSession::loadCover() {
 if (cover.stamp == 0) return;
 cover.stamp = 0;
 for (const char* ext : {"", ".png", ".jpg", ".jpeg"}) {
  std::string file = coverFile + ext;
  if (coverFile.empty() || !fs::exists(file)) continue;
  int w = 0, h = 0, c = 0;
  if (!stbi_info(file.c_str(), &w, &h, &c) || w > 2048 || h > 2048) continue;
  unsigned char* data = stbi_load(file.c_str(), &w, &h, &c, 4);
  if (!data) continue;
  if (SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888)) {
   cover.t = SDL_CreateTextureFromSurface(r, s);
   cover.w = w; cover.h = h;
   SDL_FreeSurface(s);
  }
  stbi_image_free(data);
  break;
 }
}

void GameSession::toast(const std::string& message, Icon icon) {
 toastText = message;
 toastIcon = icon;
 toastUntil = SDL_GetTicks64() + 2200;
}

// ------------------------------------------------------------------ video helpers
SDL_Rect GameSession::gameRect(int w, int h, double aspect) const {
 double ratio = aspect > 0 ? aspect : double(w) / std::max(1, h);
 if (settings.screen == 2) return {0, 0, 1280, 720};
 if (settings.screen == 1 && h > 0 && !opt.n64) {
  int s = std::max(1, 720 / h);
  int width = int(h * s * ratio + .5);
  while (s > 1 && width > 1280) { --s; width = int(h * s * ratio + .5); }
  int height = h * s;
  if (width <= 1280) return {(1280 - width) / 2, (720 - height) / 2, width, height};
 }
 int width = std::min(1280, int(720 * ratio)), height = int(width / ratio);
 return {(1280 - width) / 2, (720 - height) / 2, width, height};
}

SDL_ScaleMode GameSession::scaleMode() const { return settings.look == 1 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest; }

void GameSession::drawLook(const SDL_Rect& dst, int sourceHeight) {
 if (settings.look < 2 || sourceHeight <= 0 || !opt.looks) return;
 float step = float(dst.h) / sourceHeight;
 if (step < 2.f) return;
 std::vector<SDL_Rect> lines;
 int thick = settings.look == 3 ? std::max(1, int(step / 2)) : 1;
 for (float y = dst.y + step - thick; y < dst.y + dst.h; y += step) lines.push_back({dst.x, int(y), dst.w, thick});
 if (settings.look == 2) for (float x = dst.x + step - 1; x < dst.x + dst.w; x += step) lines.push_back({int(x), dst.y, 1, dst.h});
 SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
 SDL_SetRenderDrawColor(r, 0, 0, 0, settings.look == 3 ? 90 : 46);
 if (!lines.empty()) SDL_RenderFillRects(r, lines.data(), int(lines.size()));
}

float GameSession::audioScale() const {
 if (open || rewindActive) return 0.f;
 if (ffActive && settings.muteFastForward) return 0.f;
 return volumeScale(settings.volume);
}

// ------------------------------------------------------------------ drawing
void GameSession::drawOverlay() {
 auto& p = *paint;
 Uint64 now = SDL_GetTicks64();
 const SDL_Color glass{28, 29, 33, 215}, white{255, 255, 255, 255};
 auto pill = [&](int x, int y, Icon icon, const std::string& text, bool center) {
  int w = p.measure(text, 17, ui::Bold) + 62;
  if (center) x -= w / 2;
  p.round(x, y, w, 46, 12, glass);
  p.icon(icon, x + 28, y + 23, 22, white);
  p.text(text, x + 46, y + 13, 17, white, ui::Bold);
 };
 if (open) {
  // In the menu only the confirmation message shows, in the pane's empty top-right corner.
  if (now < toastUntil && !toastText.empty()) { int w = p.measure(toastText, 17, ui::Bold) + 62; pill(1224 - w, 50, toastIcon, toastText, false); }
  return;
 }
 if (ffActive) pill(32, 642, Icon::FastForward, fastForwardLabel(settings.ffSpeed), false);
 if (rewindActive) {
  pill(640, 590, Icon::Rewind, "Rewinding", true);
  size_t maxSnapshots = size_t(20 * 60 / rewindInterval);
  float left = std::min(1.f, float(rewindBuffer.size()) / maxSnapshots);
  p.round(360, 654, 560, 8, 4, {255, 255, 255, 64});
  p.round(360, 654, std::max(8, int(560 * left)), 8, 4, white);
 }
 if (now < toastUntil && !toastText.empty()) {
  int w = p.measure(toastText, 17, ui::Bold) + 62;
  pill(1248 - w, 642, toastIcon, toastText, false);
 }
 if (now < tipUntil) {
  std::string a = "Press ", b = " together for the menu";
  int wa = p.measure(a, 16, ui::Bold), wb = p.measure(b, 16, ui::Bold), total = wa + wb + 30 + 8 + 30 + 40;
  int x = 640 - total / 2;
  p.round(x, 28, total, 46, 12, glass);
  p.text(a, x + 20, 41, 16, white, ui::Bold);
  int bx = x + 20 + wa;
  bx += p.button("+", bx, 51, white, {28, 29, 33, 255}) + 4;
  bx += p.button("-", bx, 51, white, {28, 29, 33, 255}) + 4;
  p.text(b, bx + 2, 41, 16, white, ui::Bold);
 }
}

void GameSession::segment(int x, int y, int w, const std::vector<std::string>& labels, int selected, bool focused) {
 auto& p = *paint;
 auto& t = p.theme;
 int total = 8;
 std::vector<int> widths;
 for (auto& l : labels) { widths.push_back(p.measure(l, 15, ui::Bold) + 30); total += widths.back(); }
 int sx = x + w - total;
 p.round(sx, y, total, 40, 20, t.surf2);
 int cx = sx + 4;
 for (size_t i = 0; i < labels.size(); ++i) {
  bool on = int(i) == selected;
  if (on) p.round(cx, y + 4, widths[i], 32, 16, focused ? t.acc : t.surf);
  p.text(labels[i], cx, y + 12, 15, on ? (focused ? t.accText : t.text) : t.muted, ui::Bold, widths[i], ui::AlignCenter);
  cx += widths[i];
 }
}

void GameSession::toggle(int x, int y, bool on) {
 auto& p = *paint;
 p.round(x, y, 56, 32, 16, on ? p.theme.acc : p.theme.surf2);
 p.circle(on ? x + 40 : x + 16, y + 16, 12, {255, 255, 255, 255});
}

void GameSession::drawFooter(int x, int y, int w, const std::vector<std::pair<std::string, std::string>>& hints) {
 auto& p = *paint;
 p.fill(x, y, w, 2, p.theme.line);
 int cx = x + w - 24;
 for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
  int lw = p.measure(it->second, 16, ui::Bold);
  cx -= lw;
  p.text(it->second, cx, y + 20, 16, p.theme.text, ui::Bold);
  int bw = it->first.size() > 1 ? std::max(30, p.measure(it->first, 14, ui::Heavy) + 16) : 30;
  cx -= bw + 9;
  p.button(it->first, cx, y + 29, p.theme.btn, p.theme.btnText);
  cx -= 26;
 }
}

void GameSession::drawMenu(const std::function<void()>& drawGame) {
 auto& p = *paint;
 if (drawGame) drawGame();
 else { SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r); }
 p.fill(0, 0, 1280, 720, {0, 0, 0, 120});
 drawList(28, 28, 360, 664);
 drawPane(412, 28, 840, 664);
 if (confirm != NoDialog) drawDialog();
 drawOverlay();
}

void GameSession::drawList(int x, int y, int w, int h) {
 auto& p = *paint;
 auto& t = p.theme;
 int rad = p.radius(20);
 p.shadow(x, y, w, h, rad, 3);
 p.round(x, y, w, h, rad, t.surf);
 loadCover();
 if (cover.t) p.picture(cover.t, cover.w, cover.h, x + 20, y + 20, 56, 56, p.radius(9), t.surf);
 else {
  p.round(x + 20, y + 20, 56, 56, p.radius(9), ui::mix(ui::consoleColor(int(platform)), t.surf, .15f));
  p.text(ui::initials(title), x + 20, y + 36, 22, {255, 255, 255, 255}, ui::Heavy, 56, ui::AlignCenter);
 }
 p.text(title, x + 90, y + 24, 18, t.text, ui::Heavy, w - 110);
 long long session = (long long)(playMs / 1000);
 p.text(session < 60 ? std::string("Paused \xC2\xB7 just started") : "Paused \xC2\xB7 " + formatPlaytime(session) + " this session", x + 90, y + 50, 14, t.muted, ui::Medium, w - 110);
 p.fill(x, y + 96, w, 2, t.line);
 int iy = y + 108;
 for (int i = 0; i < ItemCount; ++i) {
  if (i == Restart) { p.fill(x + 20, iy + 4, w - 40, 2, t.line); iy += 12; }
  bool sel = i == item;
  int rx = x + 12, rw = w - 24, rh = 42;
  if (sel && !paneFocus) { p.cursor(rx, iy, rw, rh, p.radius(10), t.surf); p.round(rx, iy, rw, rh, p.radius(10), t.surf); }
  else if (sel) p.round(rx, iy, rw, rh, p.radius(10), t.surf2);
  SDL_Color ink = i == Quit ? ui::rgba(0xD9463B) : t.text;
  p.icon(itemIcon(i), rx + 26, iy + 21, 22, sel ? (i == Quit ? ink : t.acc) : (i == Quit ? ink : t.muted));
  p.text(itemName(i), rx + 50, iy + 11, 17, ink, ui::Bold, rw - 120);
  std::string value;
  if (i == Rewind) value = !opt.rewind ? "Off" : settings.rewind ? "On" : "Off";
  if (i == Speed) value = ffActive ? fastForwardLabel(settings.ffSpeed) : "1\xC3\x97";
  if (i == Cheats) { int on = 0; for (auto& c : cheats) on += c.enabled; value = opt.cheats ? (on ? std::to_string(on) + " on" : "") : "N/A"; }
  if (!value.empty()) p.text(value, rx + rw - 110, iy + 12, 15, t.muted, ui::Medium, 96, ui::AlignRight);
  iy += rh + 2;
 }
 p.fill(x, y + h - 52, w, 2, t.line);
 std::time_t now = std::time(nullptr);
 char clock[16];
 std::strftime(clock, sizeof clock, "%I:%M %p", std::localtime(&now));
 std::string c = clock;
 if (c[0] == '0') c.erase(0, 1);
 p.text(c, x + 22, y + h - 36, 15, t.muted, ui::Bold);
 p.text(platformInfo(platform).name, x + w - 222, y + h - 36, 15, t.muted, ui::Bold, 200, ui::AlignRight);
}

void GameSession::drawSlots(int x, int y, int w, bool loading) {
 auto& p = *paint;
 auto& t = p.theme;
 int gap = 18, cw = (w - 2 * gap) / 3, ph = cw * 2 / 3, ch = ph + 58;
 for (int i = 0; i < slotCount; ++i) {
  int cx = x + (i % 3) * (cw + gap), cy = y + (i / 3) * (ch + gap);
  bool sel = paneFocus && row == i, exists = slotTime(prefix, i) != 0;
  int rad = p.radius(12);
  bool disabled = !loading && i == 0;
  if (sel) p.cursor(cx, cy, cw, ch, rad, t.bg);
  if (exists) {
   p.shadow(cx, cy, cw, ch, rad, 1);
   p.round(cx, cy, cw, ch, rad, t.surf);
   auto& pic = slotPicture(i);
   if (pic.t) {
    SDL_SetTextureScaleMode(pic.t, SDL_ScaleModeNearest);
    SDL_Rect dst{cx, cy, cw, ph};
    SDL_RenderCopy(r, pic.t, nullptr, &dst);
    p.maskCorners(cx, cy, cw, ph + rad, rad, t.surf);
    p.fill(cx, cy + ph, cw, rad, t.surf);
   } else p.round(cx, cy, cw, ph, rad, t.surf2);
   p.text(slotName(i), cx + 14, cy + ph + 8, 16, disabled ? t.muted : t.text, ui::Heavy, cw - 80);
   if (i == 0) { int tw = p.measure("AUTO", 11, ui::Heavy) + 14; p.round(cx + cw - tw - 12, cy + ph + 10, tw, 20, 6, t.surf2); p.text("AUTO", cx + cw - tw - 12, cy + ph + 13, 11, t.muted, ui::Heavy, tw, ui::AlignCenter); }
   p.text(clockText(slotTime(prefix, i)), cx + 14, cy + ph + 32, 14, t.muted, ui::Medium, cw - 28);
  } else {
   SDL_Color line = t.line;
   p.round(cx, cy, cw, ch, rad, sel ? t.surf : line);
   if (!sel) p.round(cx + 2, cy + 2, cw - 4, ch - 4, std::max(0, rad - 2), t.bg);
   std::string label = i == 0 ? "Made when you quit" : (sel && !loading ? "Save to " + slotName(i) : slotName(i) + ": empty");
   if (sel && !loading && i > 0) p.icon(Icon::Plus, cx + cw / 2, cy + ch / 2 - 16, 28, t.text);
   p.text(label, cx, cy + ch / 2 + (sel && !loading && i > 0 ? 6 : -9), 15, sel ? t.text : t.muted, ui::Bold, cw, ui::AlignCenter);
  }
 }
}

void GameSession::drawRows(int x, int y, int w) {
 auto& p = *paint;
 auto& t = p.theme;
 struct RowDef { std::string label, sub; int kind; std::vector<std::string> options; int value; bool enabled; };
 std::vector<RowDef> rows;
 std::vector<std::string> applyTo = {"This game", "All games"};
 if (item == Rewind) {
  rows.push_back({"Rewind", opt.rewind ? "Hold the left stick button (L3) to go back up to 20 seconds" : "Not available for this console", 1, {}, settings.rewind && opt.rewind, opt.rewind});
 } else if (item == Speed) {
  rows.push_back({"Settings apply to", perGame ? "Only this game" : "Every game without its own settings", 0, applyTo, perGame ? 0 : 1, true});
  rows.push_back({"Fast forward speed", opt.hotkeys ? "Right stick button (R3) while playing" : "Turn it on below while playing", 0, {"1.5\xC3\x97", "2\xC3\x97", "3\xC3\x97", "4\xC3\x97"}, settings.ffSpeed, true});
  rows.push_back({"Fast forward button", "Hold to speed up, or press to switch on and off", 0, {"Hold", "Toggle"}, settings.ffToggle, opt.hotkeys});
  rows.push_back({"Fast forward now", "Stays on until you turn it off", 1, {}, ffHeldPrev, true});
  rows.push_back({"Mute while fast forwarding", "Avoids sped-up sound", 1, {}, settings.muteFastForward, true});
  rows.push_back({"Volume", "Game sound level", 0, {"Mute", "50%", "75%", "100%"}, settings.volume, true});
  rows.push_back({"Performance boost", opt.setBoost ? "Faster CPU while you play. Uses more battery and runs warmer." : "Not available here", 1, {}, settings.boost, bool(opt.setBoost)});
 } else if (item == Display) {
  rows.push_back({"Settings apply to", perGame ? "Only this game" : "Every game without its own settings", 0, applyTo, perGame ? 0 : 1, true});
  if (opt.dualScreen && opt.dsLayout) rows.push_back({"Screen layout", "Also: click the left stick (L3) while playing", 0, {"Side by side", "Stacked", "Large top"}, *opt.dsLayout % 3, true});
  else rows.push_back({"Screen size", opt.screenModes ? "Pixel perfect keeps every pixel the same size" : "Not available for this console", 0, opt.n64 ? std::vector<std::string>{"Fit", "Stretch"} : std::vector<std::string>{"Fit", "Pixel perfect", "Stretch"}, opt.n64 ? (settings.screen == 2 ? 1 : 0) : settings.screen, opt.screenModes});
  rows.push_back({"Look", opt.looks ? "Smooth softens pixels; LCD and CRT add a retro screen" : "Not available for this console", 0, {"Sharp", "Smooth", "LCD", "CRT"}, settings.look, opt.looks});
  if (opt.dualScreen) rows.push_back({"3D rendering", "Two cores is faster in 3D games. Applies when the game next starts.", 0, {"Two cores", "One core"}, settings.dsThreaded ? 0 : 1, true});
 }
 int ry = y;
 for (size_t i = 0; i < rows.size(); ++i) {
  auto& d = rows[i];
  bool sel = paneFocus && row == int(i);
  bool compact = rows.size() > 5;
  int rh = compact ? 56 : 66, rad = p.radius(12);
  if (sel) p.cursor(x, ry, w, rh, rad, t.bg);
  else p.shadow(x, ry, w, rh, rad, 1);
  p.round(x, ry, w, rh, rad, t.surf);
  p.text(d.label, x + 20, ry + (compact ? 8 : 12), 17, d.enabled ? t.text : t.muted, ui::Bold, w - 360);
  p.text(d.sub, x + 20, ry + (compact ? 31 : 37), 14, t.muted, ui::Medium, d.kind == 0 ? w - 40 - 330 : w - 120);
  if (d.kind == 0) segment(x, ry + (rh - 40) / 2, w - 16, d.options, d.value, sel);
  else toggle(x + w - 76, ry + (rh - 32) / 2, d.value != 0);
  ry += rh + (compact ? 8 : 12);
 }
}

void GameSession::drawPane(int x, int y, int w, int h) {
 auto& p = *paint;
 auto& t = p.theme;
 int rad = p.radius(20);
 p.shadow(x, y, w, h, rad, 3);
 p.round(x, y, w, h, rad, t.bg);
 int cx = x + 30, cw = w - 60, top = y + 28;
 std::string heading = itemName(item), sub;
 std::vector<std::pair<std::string, std::string>> hints;
 switch (item) {
 case Resume: sub = "Pick up right where you paused."; hints = {{"A", "Resume"}, {"B", "Resume"}}; break;
 case SaveState: sub = opt.states ? "Choose a slot. You'll be asked before a save is replaced." : opt.statesNote; hints = paneFocus ? std::vector<std::pair<std::string, std::string>>{{"A", "Save"}, {"Y", "Delete"}, {"B", "Back"}} : std::vector<std::pair<std::string, std::string>>{{"A", "Choose"}, {"B", "Resume"}}; break;
 case LoadState: sub = opt.states ? "Your game continues from the picture you choose." : opt.statesNote; hints = paneFocus ? std::vector<std::pair<std::string, std::string>>{{"A", "Load"}, {"Y", "Delete"}, {"B", "Back"}} : std::vector<std::pair<std::string, std::string>>{{"A", "Choose"}, {"B", "Resume"}}; if (paneFocus && !undoState.empty()) hints.insert(hints.begin() + 1, {"X", "Undo load"}); break;
 case Rewind: sub = "Made a mistake? Go back a few seconds."; hints = {{"A", "On / Off"}, {"B", "Back"}}; break;
 case Speed: sub = "Fast forward and sound."; hints = {{"A", "Change"}, {"B", "Back"}}; break;
 case Cheats: sub = opt.cheats ? (browsingFiles ? "Choose a cheat file to add its codes." : "Changes take effect when you resume.") : opt.cheatsNote; hints = browsingFiles ? std::vector<std::pair<std::string, std::string>>{{"A", "Add"}, {"B", "Back"}} : std::vector<std::pair<std::string, std::string>>{{"A", "On / Off"}, {"X", "Add code"}, {"Y", "Delete"}, {"B", "Back"}}; break;
 case Display: sub = "How the game looks on your screen."; hints = {{"A", "Change"}, {"B", "Back"}}; break;
 case Controls: sub = "Buttons while you play."; hints = {{"B", "Resume"}}; break;
 case Screenshot: sub = "Saves a picture of the game to your SD card."; hints = {{"A", "Take screenshot"}, {"B", "Resume"}}; break;
 case Restart: sub = "Start this game again from the beginning."; hints = {{"A", "Restart"}, {"B", "Resume"}}; break;
 case Quit: sub = "Saves exactly where you are, then goes back to your library."; hints = {{"A", "Save and quit"}, {"B", "Resume"}}; break;
 }
 p.text(heading, cx, top, 26, t.text, ui::Heavy, cw);
 p.text(sub, cx, top + 38, 15, t.muted, ui::Medium, cw);
 int body = top + 78;
 switch (item) {
 case Resume: {
  p.circle(x + w / 2, y + 300, 58, t.surf);
  p.icon(Icon::Play, x + w / 2 + 3, y + 300, 52, t.acc);
  p.text("Paused", x, y + 382, 24, t.text, ui::Heavy, w, ui::AlignCenter);
  p.text("Press B, or + and - together, to keep playing.", x, y + 418, 16, t.muted, ui::Medium, w, ui::AlignCenter);
  break;
 }
 case SaveState: case LoadState:
  if (opt.states) drawSlots(cx, body, cw, item == LoadState);
  else p.paragraph(opt.statesNote, cx, body, 17, t.muted, cw, 26);
  break;
 case Rewind: case Speed: case Display: drawRows(cx, body, cw); break;
 case Cheats: {
  if (!opt.cheats) { p.paragraph(opt.cheatsNote, cx, body, 17, t.muted, cw, 26); break; }
  int count = rowCount(), visible = 6, first = std::clamp(row - visible + 2, 0, std::max(0, count - visible));
  int ry = body;
  for (int i = first; i < count && i < first + visible; ++i) {
   bool sel = paneFocus && row == i;
   int rh = 64, rad = p.radius(12);
   bool special = !browsingFiles && !browsingOnline && i >= int(cheats.size());
   if (sel) p.cursor(cx, ry, cw, rh, rad, t.bg);
   if (special) {
    p.round(cx, ry, cw, rh, rad, sel ? t.surf : t.line);
    if (!sel) p.round(cx + 2, ry + 2, cw - 4, rh - 4, std::max(0, rad - 2), t.bg);
    bool add = i == int(cheats.size());
    bool download = i == int(cheats.size())+2;
    p.icon(add ? Icon::Keyboard : Icon::Folder, cx + 32, ry + 32, 24, sel ? t.acc : t.muted);
    p.text(download ? "Download via database" : add ? "Type a new code" : "Load from SD card", cx + 60, ry + 12, 17, t.text, ui::Bold, cw - 80);
    p.text(download ? (onlineCheats.phase()==OnlineCheats::Searching || onlineCheats.phase()==OnlineCheats::Downloading ? onlineCheats.message() : "Online catalog / cached files / codes stay disabled") : add ? "Name it, then enter the code" : "cheats/" + std::string(platformInfo(platform).slug) + "/ on your SD card", cx + 60, ry + 37, 14, t.muted, ui::Medium, cw - 80);
   } else if (browsingOnline) {
    p.round(cx,ry,cw,rh,rad,t.surf);
    p.text(onlineChoices[i].title,cx+20,ry+10,16,t.text,ui::Bold,cw-40);
    p.text(std::to_string(onlineChoices[i].count)+" codes / choose your region and revision",cx+20,ry+37,13,t.muted,ui::Medium,cw-40);
   } else if (browsingFiles) {
    if (!sel) p.shadow(cx, ry, cw, rh, rad, 1);
    p.round(cx, ry, cw, rh, rad, t.surf);
    p.icon(Icon::Folder, cx + 32, ry + 32, 24, t.muted);
    p.text(fs::path(files[i]).stem().string(), cx + 60, ry + 21, 17, t.text, ui::Bold, cw - 80);
   } else {
    if (!sel) p.shadow(cx, ry, cw, rh, rad, 1);
    p.round(cx, ry, cw, rh, rad, t.surf);
    p.text(cheats[i].description, cx + 20, ry + 12, 17, t.text, ui::Bold, cw - 120);
    p.text(cheats[i].code, cx + 20, ry + 37, 13, t.muted, ui::Mono, cw - 120);
    toggle(cx + cw - 76, ry + 16, cheats[i].enabled);
   }
   ry += rh + 10;
  }
  if (!browsingFiles && !browsingOnline && cheats.empty() && !paneFocus) p.text("No cheats yet for this game.", cx, ry + 6, 15, t.muted, ui::Medium, cw);
  break;
 }
 case Controls: {
  std::vector<std::pair<std::string, std::string>> rows = {{"+ -", "Press together to open this menu"}};
  if (opt.n64) { rows.push_back({"- ZR", "Quick save to Slot 1"}); rows.push_back({"- ZL", "Quick load Slot 1"}); }
  else { rows.push_back({"ZR", "Quick save to Slot 1"}); rows.push_back({"ZL", "Quick load Slot 1"}); }
  if (opt.hotkeys) { rows.push_back({"R3", settings.ffToggle ? "Fast forward on / off" : "Hold to fast forward"}); if (opt.rewind) rows.push_back({"L3", "Hold to rewind"}); }
  if (opt.dualScreen) { rows.push_back({"R stick", "Move the touch cursor"}); rows.push_back({"R3", "Touch the bottom screen"}); rows.push_back({"L3", "Change the screen layout"}); }
  rows.push_back({"Touch", opt.dualScreen ? "Use the bottom screen directly" : "Tap the top-right corner for the menu"});
  int ry = body;
  for (auto& rr : rows) {
   int kw = std::max(44, p.measure(rr.first, 15, ui::Heavy) + 24);
   p.round(cx, ry, kw, 34, 17, t.btn);
   p.text(rr.first, cx, ry + 8, 15, t.btnText, ui::Heavy, kw, ui::AlignCenter);
   p.text(rr.second, cx + kw + 18, ry + 7, 17, t.text, ui::Bold, cw - kw - 20);
   ry += 50;
  }
  break;
 }
 case Screenshot: case Restart: case Quit: {
  Icon big = item == Screenshot ? Icon::Camera : item == Restart ? Icon::Restart : Icon::Power;
  p.circle(x + w / 2, y + 300, 58, t.surf);
  p.icon(big, x + w / 2, y + 300, 52, item == Quit ? ui::rgba(0xD9463B) : t.acc);
  std::string line = item == Screenshot ? "Saved to screenshots/" + std::string(platformInfo(platform).slug) + "/"
                   : item == Restart ? "Anything you haven't saved will be lost."
                   : "Choose Resume on the Home screen to continue from here.";
  p.text(line, x, y + 390, 16, t.muted, ui::Medium, w, ui::AlignCenter);
  break;
 }
 }
 drawFooter(x, y + h - 60, w, hints);
}

void GameSession::drawDialog() {
 auto& p = *paint;
 auto& t = p.theme;
 p.fill(0, 0, 1280, 720, {0, 0, 0, 110});
 int w = 560, h = 230, x = (1280 - w) / 2, y = (720 - h) / 2, rad = p.radius(20);
 p.shadow(x, y, w, h, rad, 4);
 p.round(x, y, w, h, rad, t.surf);
 std::string head, body, yes;
 switch (confirm) {
 case OverwriteSlot: head = "Replace " + slotName(confirmSlot) + "?"; body = "The save already in this slot will be replaced."; yes = "Replace"; break;
 case DeleteSlot: head = "Delete " + slotName(confirmSlot) + "?"; body = "This save can't be brought back."; yes = "Delete"; break;
 case RestartGame: head = "Restart the game?"; body = "Anything you haven't saved will be lost."; yes = "Restart"; break;
 case DeleteCheat: head = "Delete this cheat?"; body = confirmSlot < int(cheats.size()) ? cheats[confirmSlot].description : ""; yes = "Delete"; break;
 }
 p.text(head, x + 32, y + 32, 24, t.text, ui::Heavy, w - 64);
 p.paragraph(body, x + 32, y + 76, 16, t.muted, w - 64, 24, 2);
 p.fill(x, y + h - 70, w, 2, t.line);
 int bx = x + w - 32;
 int lw = p.measure("Cancel", 16, ui::Bold);
 bx -= lw; p.text("Cancel", bx, y + h - 45, 16, t.text, ui::Bold); bx -= 39; p.button("B", bx, y + h - 36, t.btn, t.btnText);
 bx -= 28; lw = p.measure(yes, 16, ui::Bold); bx -= lw; p.text(yes, bx, y + h - 45, 16, t.text, ui::Bold); bx -= 39; p.button("A", bx, y + h - 36, t.acc, t.accText);
}
