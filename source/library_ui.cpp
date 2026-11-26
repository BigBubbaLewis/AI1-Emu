// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
#include "library_ui.h"
#include "app_version.h"
#include "game_database.h"
#include "platform_library.h"
#include "runtime_paths.h"
#include "third_party/stb_image.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
namespace fs = std::filesystem;
using ui::Icon;

namespace {
std::string base;
std::string stem(const std::string& p) { return fs::path(p).stem().string(); }
std::string shortTitle(const std::string& title) { auto cut = title.find(" ("); return cut == std::string::npos ? title : title.substr(0, cut); }
std::string lower(std::string s) { for (char& c : s) c = char(std::tolower((unsigned char)c)); return s; }
std::string commas(long long n) { std::string s = std::to_string(n); for (int i = int(s.size()) - 3; i > 0; i -= 3) s.insert(size_t(i), ","); return s; }
std::string bytesLabel(uintmax_t n) {
 char b[32];
 if (n >= 1073741824ULL) std::snprintf(b, sizeof b, "%.1f GB", n / 1073741824.0);
 else if (n >= 1048576ULL) std::snprintf(b, sizeof b, "%.1f MB", n / 1048576.0);
 else if (n >= 1024) std::snprintf(b, sizeof b, "%.0f KB", n / 1024.0);
 else std::snprintf(b, sizeof b, "%u B", unsigned(n));
 return b;
}
bool cachedImageAvailable(const std::string& path) {
 for (const char* ext : {".png", ".jpg", ".jpeg"}) { int w = 0, h = 0, c = 0; if (stbi_info((path + ext).c_str(), &w, &h, &c) && w > 0 && h > 0 && w <= 2048 && h <= 2048) return true; }
 return false;
}
std::string clockNow() {
 std::time_t now = std::time(nullptr);
 char b[16];
 std::strftime(b, sizeof b, "%I:%M %p", std::localtime(&now));
 std::string s = b;
 if (!s.empty() && s[0] == '0') s.erase(0, 1);
 return s;
}
const char* sectionName(int s) {
 static const char* n[] = {"Library", "Gameplay", "Controls", "Themes", "Updates", "Game map", "About"};
 return n[s];
}
Icon sectionIcon(int s) {
 static const Icon i[] = {Icon::Grid, Icon::Play, Icon::Pad, Icon::Palette, Icon::Update, Icon::Map, Icon::Info};
 return i[s];
}
const char* sortName(int s) { static const char* n[] = {"Recently played", "Title A\xE2\x80\x93Z", "Title Z\xE2\x80\x93" "A", "Most played"}; return n[std::clamp(s, 0, 3)]; }
const std::vector<std::string> accentChoices = {"", "#1E9BE6", "#E0517F", "#2E9E5B", "#E9A23B", "#7B5CD6", "#D9463B", "#2B2C30"};
} // namespace

// ------------------------------------------------------------------ setup
LibraryUI::LibraryUI(SDL_Renderer* renderer) : r(renderer), paint(renderer) {
 root = retroverseRoot();
 base = root;
 storageRoot = root;
 settings = loadSettings(root);
 applyTheme();
 history = loadHistory(root);
 favorites = loadFavorites(root);
 prewarmFonts();
 if (r) artThread = SDL_CreateThread(artWorker, "ai1-covers", this);
}

LibraryUI::~LibraryUI() {
 stopStorageScan();
 { std::lock_guard<std::mutex> lock(artMutex); artStop = true; }
 artWake.notify_all();
 if (artThread) SDL_WaitThread(artThread, nullptr);
 for (auto& a : images) if (a.second.texture) SDL_DestroyTexture(a.second.texture);
 if (wallpaper.texture) SDL_DestroyTexture(wallpaper.texture);
}

// Text atlases are built the first time a size is used, which can take a frame or two
// on a Switch. Build the sizes the main screens use up front, so the first visit to a
// tab does not stall.
void LibraryUI::prewarmFonts() {
 if (!r) return;
 const int medium[] = {13, 14, 15, 16, 17, 18}, bold[] = {13, 14, 15, 16, 17, 18, 19, 21}, heavy[] = {14, 19, 21, 22, 23, 24, 26, 36};
 for (int s : medium) paint.measure("A", s, ui::Medium);
 for (int s : bold) paint.measure("A", s, ui::Bold);
 for (int s : heavy) paint.measure("A", s, ui::Heavy);
}

void LibraryUI::applyTheme() {
 paint.setTheme(resolveTheme(settings));
 paint.family = settings.font;
 paint.corners = settings.corners;
 wallpaperTried = false;
 if (wallpaper.texture) { SDL_DestroyTexture(wallpaper.texture); wallpaper = {}; }
}

void LibraryUI::persistSettings() { saveSettings(root, settings); }

void LibraryUI::reloadData() {
 history = loadHistory(root);
 favorites = loadFavorites(root);
 settings = loadSettings(root);
 applyTheme();
 infoCache.clear();
 rebuild();
}

void LibraryUI::setGames(const std::vector<std::string>& games) {
 all = games;
 coverScan = 0;
 // One-time import of v0.4 ".lastplayed" stamps so Home is not empty after updating.
 std::error_code e;
 if (!fs::exists(root + "history.txt", e)) {
  for (auto& g : all) {
   std::ifstream in(gameSavePrefix(root, g) + ".lastplayed");
   long long stamp = 0;
   if (in >> stamp && stamp > 0) recordSession(root, g, stamp, 0);
  }
  history = loadHistory(root);
 }
 // Consoles with games come first; the rest follow so every console has a place
 // that explains where its games go (for example 3DS before any game is added).
 consoles.clear();
 std::vector<int> empty;
 for (auto& info : platforms()) {
  bool has = false;
  for (auto& g : all) if (detectPlatform(g) == info.id) { has = true; break; }
  (has ? consoles : empty).push_back(int(info.id));
 }
 consoles.insert(consoles.end(), empty.begin(), empty.end());
 rebuild();
}

void LibraryUI::setQuery(const std::string& q) { query = q; gridIndex = 0; gridScroll = 0; rebuild(); }

void LibraryUI::rebuild() {
 std::string keep = selectedGame();
 auto titleOf = [](const std::string& g) { return lower(gameTitle(g)); };
 sorted = all;
 auto byTitle = [&](const std::string& a, const std::string& b) { return titleOf(a) < titleOf(b); };
 switch (settings.sort) {
 case 1: std::sort(sorted.begin(), sorted.end(), byTitle); break;
 case 2: std::sort(sorted.begin(), sorted.end(), [&](auto& a, auto& b) { return titleOf(a) > titleOf(b); }); break;
 case 3: std::sort(sorted.begin(), sorted.end(), [&](auto& a, auto& b) { long long x = history.count(a) ? history[a].seconds : 0, y = history.count(b) ? history[b].seconds : 0; return x != y ? x > y : byTitle(a, b); }); break;
 default: std::sort(sorted.begin(), sorted.end(), [&](auto& a, auto& b) { long long x = history.count(a) ? history[a].lastPlayed : 0, y = history.count(b) ? history[b].lastPlayed : 0; return x != y ? x > y : byTitle(a, b); }); break;
 }
 view.clear();
 std::string q = lower(query);
 for (auto& g : sorted) {
  if (filter == 1 && !favorites.count(g)) continue;
  if (filter >= 2 && (filter - 2 >= int(consoles.size()) || int(detectPlatform(g)) != consoles[filter - 2])) continue;
  if (!q.empty() && titleOf(g).find(q) == std::string::npos) continue;
  view.push_back(g);
 }
 recent.clear();
 for (auto& g : all) if (history.count(g) && history[g].lastPlayed > 0) recent.push_back(g);
 std::sort(recent.begin(), recent.end(), [&](auto& a, auto& b) { return history[a].lastPlayed > history[b].lastPlayed; });
 if (!keep.empty()) select(keep);
 gridIndex = std::min(gridIndex, view.empty() ? 0 : view.size() - 1);
 shelfIndex = std::min(shelfIndex, sorted.empty() ? 0 : sorted.size() - 1);
}

std::string LibraryUI::selectedGame() const {
 if (tab == TabHome) {
  if (homeZone == 2 && shelfIndex < sorted.size()) return sorted[shelfIndex];
  if (homeZone == 1 && size_t(homeIndex + 1) < recent.size()) return recent[size_t(homeIndex + 1)];
  if (!recent.empty()) return recent[0];
  return sorted.empty() ? "" : sorted[0];
 }
 return gridIndex < view.size() ? view[gridIndex] : "";
}

void LibraryUI::select(const std::string& game) {
 auto it = std::find(view.begin(), view.end(), game);
 if (it != view.end()) gridIndex = size_t(it - view.begin());
 auto jt = std::find(sorted.begin(), sorted.end(), game);
 if (jt != sorted.end()) shelfIndex = size_t(jt - sorted.begin());
}

const LibraryUI::GameInfo& LibraryUI::infoFor(const std::string& game) {
 // Read from the SD card once per game; reloadData() clears the cache after a game
 // closes, which is the only time states or cheats change. (Re-reading every few
 // seconds caused a visible stutter.)
 auto& i = infoCache[game];
 Uint64 now = SDL_GetTicks64();
 if (i.at) return i;
 std::string prefix = gameSavePrefix(root, game);
 i.states = usedSlots(prefix);
 i.autoSave = slotTime(prefix, 0) != 0;
 i.cheatsOn = 0;
 for (auto& c : loadCheats(cheatPath(root, game))) i.cheatsOn += c.enabled;
 i.at = now ? now : 1;
 return i;
}
bool LibraryUI::hasAutoSave(const std::string& game) { return !game.empty() && infoFor(game).autoSave; }

void LibraryUI::toggleFavorite(const std::string& game) {
 if (game.empty()) return;
 if (favorites.count(game)) { favorites.erase(game); toast("Removed from favourites"); }
 else { favorites.insert(game); toast("Added to favourites"); }
 saveFavorites(root, favorites);
 if (filter == 1) rebuild();
}

void LibraryUI::launch(const std::string& game, bool resume) {
 if (game.empty()) return;
 pending.type = Command::Launch;
 pending.path = game;
 pending.resume = resume && hasAutoSave(game);
}

void LibraryUI::toast(const std::string& message) { toastText = message; toastUntil = SDL_GetTicks64() + 3200; }

// ------------------------------------------------------------------ artwork
std::string LibraryUI::coverFor(const std::string& p) {
 static std::map<std::string, std::string> paths;
 auto it = paths.find(p);
 if (it != paths.end()) return it->second;
 auto platform = detectPlatform(p);
 std::string slug = platformInfo(platform).slug, custom = base + "covers/" + slug + "/" + stem(p);
 for (const char* ext : {".png", ".jpg", ".jpeg"}) if (fs::exists(custom + ext)) return paths[p] = custom;
 if (platform == Platform::GBA) for (const char* ext : {".png", ".jpg", ".jpeg"}) if (fs::exists(base + "covers/" + stem(p) + ext)) return paths[p] = base + "covers/" + stem(p);
 auto name = gameTitle(p);
 for (char& c : name) if (std::string("&*/:`<>?\\|").find(c) != std::string::npos) c = '_';
 // Your own cover may also be named after the game's title instead of its file name.
 if (name != stem(p)) for (const char* ext : {".png", ".jpg", ".jpeg"}) if (fs::exists(base + "covers/" + slug + "/" + name + ext)) return paths[p] = base + "covers/" + slug + "/" + name;
 // Or a similar name: tags, case, accents, punctuation and version numbers are ignored
 // (see looseNameKey), so "Pokemon_Unbound_v2.1.gba" finds "Pokémon Unbound.png".
 {
  static std::map<std::string, std::vector<std::string>> folders; // folder -> picture names (no extension)
  std::vector<std::string> dirs = {base + "covers/" + slug + "/"};
  if (platform == Platform::GBA) dirs.push_back(base + "covers/");
  for (auto& dir : dirs) {
   auto it = folders.find(dir);
   if (it == folders.end()) {
    std::vector<std::string> list;
    std::error_code ec;
    for (fs::directory_iterator d(dir, ec), end; !ec && d != end; d.increment(ec)) {
     if (!d->is_regular_file(ec)) continue;
     auto ext = lower(d->path().extension().string());
     if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") list.push_back(d->path().stem().string());
    }
    it = folders.emplace(dir, std::move(list)).first;
   }
   int found = matchCover({stem(p), gameTitle(p)}, it->second);
   if (found >= 0) return paths[p] = dir + it->second[size_t(found)];
  }
 }
 if (platform == Platform::GBA && name == "Metroid - Zero Mission (USA, Australia)") name = "Metroid - Zero Mission (USA)";
 if (platform == Platform::GBA && cachedImageAvailable(base + "artwork-cache/" + name)) return paths[p] = base + "artwork-cache/" + name;
 if ((platform == Platform::GB || platform == Platform::GBC) && !cachedImageAvailable(base + "artwork-cache/" + slug + "/" + name)) {
  std::string other = platform == Platform::GB ? "gbc" : "gb";
  if (cachedImageAvailable(base + "artwork-cache/" + other + "/" + name)) return paths[p] = base + "artwork-cache/" + other + "/" + name;
 }
 return paths[p] = base + "artwork-cache/" + slug + "/" + name;
}

LibraryUI::Art LibraryUI::loadImage(const std::string& path, int maxDim) {
 Art a;
 if (!r) return a;
 for (const char* ext : {".png", ".jpg", ".jpeg"}) {
  std::string name = path + ext;
  std::ifstream file(name, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() > 8 * 1024 * 1024) continue;
  file.close();
  int ch = 0, w = 0, h = 0;
  if (!stbi_info(name.c_str(), &w, &h, &ch) || w > 4096 || h > 4096) continue;
  auto* data = stbi_load(name.c_str(), &w, &h, &ch, 4);
  if (!data) continue;
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
  if (surface) {
   float scale = std::min(1.f, float(maxDim) / std::max(w, h));
   a.w = std::max(1, int(w * scale)); a.h = std::max(1, int(h * scale));
   SDL_Surface* small = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ABGR8888);
   if (small) { SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE); SDL_BlitScaled(surface, nullptr, small, nullptr); a.texture = SDL_CreateTextureFromSurface(r, small); SDL_FreeSurface(small); }
   SDL_FreeSurface(surface);
  }
  stbi_image_free(data);
  break;
 }
 return a;
}

// Returns the cover at once: ready, still loading (state 0) or missing (state 2).
// Decoding happens on the cover thread; pumpArt() uploads finished ones.
LibraryUI::Art& LibraryUI::art(const std::string& path) {
 auto it = images.find(path);
 if (it != images.end()) { it->second.used = tick; return it->second; }
 if (images.size() >= 80) {
  auto oldest = images.end();
  for (auto jt = images.begin(); jt != images.end(); ++jt)
   if (jt->second.state != 0 && (oldest == images.end() || jt->second.used < oldest->second.used)) oldest = jt;
  if (oldest != images.end()) { if (oldest->second.texture) SDL_DestroyTexture(oldest->second.texture); images.erase(oldest); }
 }
 Art a;
 a.used = tick;
 if (!r || !artThread) { a = loadImage(path, 320); a.used = tick; a.state = a.texture ? 1 : 2; return images.emplace(path, a).first->second; }
 a.state = 0;
 { std::lock_guard<std::mutex> lock(artMutex); artRequests.push_back(path); }
 artWake.notify_one();
 return images.emplace(path, a).first->second;
}

int LibraryUI::artWorker(void* self) {
 auto* ui = static_cast<LibraryUI*>(self);
 for (;;) {
  std::string path;
  {
   std::unique_lock<std::mutex> lock(ui->artMutex);
   ui->artWake.wait(lock, [ui] { return ui->artStop.load() || !ui->artRequests.empty(); });
   if (ui->artStop) return 0;
   // Newest request first: what is on screen now matters more than what scrolled past.
   path = ui->artRequests.back();
   ui->artRequests.pop_back();
  }
  Decoded d;
  d.path = path;
  for (const char* ext : {".png", ".jpg", ".jpeg"}) {
   std::string name = path + ext;
   std::ifstream file(name, std::ios::binary | std::ios::ate);
   if (!file || file.tellg() > 8 * 1024 * 1024) continue;
   file.close();
   int ch = 0, w = 0, h = 0;
   if (!stbi_info(name.c_str(), &w, &h, &ch) || w > 4096 || h > 4096) continue;
   unsigned char* data = stbi_load(name.c_str(), &w, &h, &ch, 4);
   if (!data) continue;
   // Box-filter down to at most 320 px: smooth, and small enough to upload quickly.
   float scale = std::min(1.f, 320.f / std::max(w, h));
   int tw = std::max(1, int(w * scale)), th = std::max(1, int(h * scale));
   d.rgba.assign(size_t(tw) * th * 4, 0);
   for (int y = 0; y < th; ++y) {
    int y0 = y * h / th, y1 = std::max(y0 + 1, (y + 1) * h / th);
    for (int x = 0; x < tw; ++x) {
     int x0 = x * w / tw, x1 = std::max(x0 + 1, (x + 1) * w / tw);
     unsigned sum[4] = {0, 0, 0, 0}, n = 0;
     for (int sy = y0; sy < y1; ++sy) for (int sx = x0; sx < x1; ++sx) { const unsigned char* p = data + (size_t(sy) * w + sx) * 4; for (int c = 0; c < 4; ++c) sum[c] += p[c]; ++n; }
     for (int c = 0; c < 4; ++c) d.rgba[(size_t(y) * tw + x) * 4 + c] = (unsigned char)(sum[c] / n);
    }
   }
   stbi_image_free(data);
   d.w = tw; d.h = th; d.ok = true;
   break;
  }
  { std::lock_guard<std::mutex> lock(ui->artMutex); ui->artDone.push_back(std::move(d)); }
 }
}

void LibraryUI::pumpArt(int maxUploads) {
 for (int n = 0; n < maxUploads; ++n) {
  Decoded d;
  { std::lock_guard<std::mutex> lock(artMutex); if (artDone.empty()) return; d = std::move(artDone.front()); artDone.pop_front(); }
  auto it = images.find(d.path);
  if (it == images.end()) continue;
  Art& a = it->second;
  if (!d.ok) { a.state = 2; continue; }
  a.texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, d.w, d.h);
  if (a.texture) { SDL_UpdateTexture(a.texture, nullptr, d.rgba.data(), d.w * 4); SDL_SetTextureBlendMode(a.texture, SDL_BLENDMODE_BLEND); a.w = d.w; a.h = d.h; a.state = 1; }
  else a.state = 2;
 }
}

std::vector<std::pair<std::string, std::string>> LibraryUI::missingCovers(int budget) {
 std::vector<std::pair<std::string, std::string>> out;
 static const char* hexDigits = "0123456789ABCDEF";
 for (; budget > 0 && coverScan < all.size(); --budget) {
  const std::string& game = all[coverScan++];
  const char* repo = thumbnailRepository(detectPlatform(game));
  if (!repo) continue;
  std::string dest = coverFor(game);
  // Only fill the download cache; covers/ holds the owner's own pictures.
  if (dest.rfind(base + "artwork-cache/", 0) != 0) continue;
  if (cachedImageAvailable(dest) || fs::exists(dest + ".none")) continue;
  bool queued = false;
  for (auto& o : out) if (o.second == dest) queued = true;
  if (queued) continue;
  std::string name = fs::path(dest).filename().string() + ".png", encoded;
  for (unsigned char c : name) {
   if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += char(c);
   else { encoded += '%'; encoded += hexDigits[c >> 4]; encoded += hexDigits[c & 15]; }
  }
  out.push_back({"https://raw.githubusercontent.com/libretro-thumbnails/" + std::string(repo) + "/master/Named_Boxarts/" + encoded, dest});
 }
 return out;
}

void LibraryUI::coverArrived(const std::string& path) {
 auto it = images.find(path);
 if (it == images.end()) return;
 if (it->second.texture) SDL_DestroyTexture(it->second.texture);
 images.erase(it); // drawn again on the next frame, now from the downloaded file
}

bool LibraryUI::artIdle() {
 std::lock_guard<std::mutex> lock(artMutex);
 if (!artRequests.empty() || !artDone.empty()) return false;
 for (auto& a : images) if (a.second.state == 0) return false;
 return true;
}

// ------------------------------------------------------------------ input
LibraryUI::Command LibraryUI::update(uint32_t down, uint32_t held) {
 pending = {};
 Uint64 now = SDL_GetTicks64();
 const uint32_t dirs = pad::Up | pad::Down | pad::Left | pad::Right;
 uint32_t heldDirs = held & dirs;
 if (heldDirs != repeatBits) { down |= heldDirs & ~repeatBits; repeatBits = heldDirs; repeatAt = now + 320; }
 else if (heldDirs && now >= repeatAt) { down |= heldDirs; repeatAt = now + 90; }
 if (!down) return pending;
 if (!(down & pad::B)) quitArmed = false;

 bool capture = tab == TabSettings && section == SecControls && sectionFocus;
 if (capture) { if (down & pad::B) sectionFocus = false; return pending; }

 if (down & pad::L) { tab = (tab + tabCount - 1) % tabCount; return pending; }
 if (down & pad::R) { tab = (tab + 1) % tabCount; return pending; }
 if (down & pad::Plus) { tab = tab == TabSettings ? TabHome : TabSettings; return pending; }

 if (tab == TabHome) moveHome(down);
 else if (tab == TabLibrary) moveGrid(down);
 else moveSettings(down);
 return pending;
}

void LibraryUI::moveHome(uint32_t down) {
 std::string hero = !recent.empty() ? recent[0] : sorted.empty() ? "" : sorted[0];
 int heroButtons = hasAutoSave(hero) ? 2 : 1;
 int recentRows = int(std::min<size_t>(3, recent.size() > 1 ? recent.size() - 1 : 0));
 if (down & pad::B) {
  if (quitArmed) { pending.type = Command::Quit; return; }
  quitArmed = true;
  toast("Press B again to close AI1 Emu");
  return;
 }
 if (down & pad::X) { tab = TabLibrary; pending.type = Command::Search; return; }
 if (down & pad::Y) { toggleFavorite(selectedGame()); return; }
 if (sorted.empty()) return;
 if (homeZone == 0) {
  if (down & pad::Left) homeIndex = std::max(0, homeIndex - 1);
  if (down & pad::Right) { if (homeIndex + 1 < heroButtons) ++homeIndex; else if (recentRows) { homeZone = 1; homeIndex = 0; } }
  if (down & pad::Down) { homeZone = 2; }
  if (down & pad::A) launch(hero, homeIndex == 0);
 } else if (homeZone == 1) {
  if (down & pad::Up) homeIndex = std::max(0, homeIndex - 1);
  if (down & pad::Down) { if (homeIndex + 1 < recentRows) ++homeIndex; else homeZone = 2; }
  if (down & pad::Left) { homeZone = 0; homeIndex = 0; }
  if ((down & pad::A) && size_t(homeIndex + 1) < recent.size()) launch(recent[size_t(homeIndex + 1)], true);
 } else {
  if ((down & (pad::Left | pad::Right)) && !shelfFollow) {
   // After a touch swipe, continue from what is on screen instead of jumping back.
   float x = 48 + shelfIndex * 166.f - shelfScroll;
   if (x < 36 || x + 148 > 1244) shelfIndex = std::min(sorted.size() - 1, size_t(std::max(0.f, std::ceil(shelfScroll / 166.f))));
   shelfFollow = true;
  }
  if ((down & pad::Left) && shelfIndex > 0) --shelfIndex;
  if ((down & pad::Right) && shelfIndex + 1 < sorted.size()) ++shelfIndex;
  if (down & pad::Up) { float x = 48 + shelfIndex * 166.f - shelfScroll; if (x > 820 && recentRows) { homeZone = 1; homeIndex = recentRows - 1; } else { homeZone = 0; homeIndex = 0; } }
  if (down & pad::A) launch(sorted[shelfIndex], false);
 }
}

void LibraryUI::moveGrid(uint32_t down) {
 const int cols = 5;
 int filters = 2 + int(consoles.size());
 if (down & pad::B) { tab = TabHome; return; }
 if (down & pad::X) { pending.type = Command::Search; return; }
 if (down & pad::ZL) { filter = (filter + filters - 1) % filters; gridIndex = 0; gridScroll = 0; gridFollow = true; gridVelocity = 0; rebuild(); return; }
 if (down & pad::ZR) { filter = (filter + 1) % filters; gridIndex = 0; gridScroll = 0; gridFollow = true; gridVelocity = 0; rebuild(); return; }
 if (down & pad::Minus) { settings.sort = (settings.sort + 1) % 4; persistSettings(); rebuild(); toast(std::string("Sorted by ") + lower(sortName(settings.sort))); return; }
 if (view.empty()) return;
 size_t n = view.size();
 if ((down & (pad::Left | pad::Right | pad::Up | pad::Down)) && !gridFollow) {
  // After a touch swipe, continue from the first row on screen instead of jumping back.
  const float rowH = 151 + 62;
  float top = float(gridIndex / cols) * rowH - gridScroll;
  if (top < -20 || top + rowH > 470 + 20) gridIndex = std::min(n - 1, size_t(std::ceil(gridScroll / rowH)) * size_t(cols));
  gridFollow = true;
  gridVelocity = 0;
 }
 if ((down & pad::Left) && gridIndex > 0) --gridIndex;
 if ((down & pad::Right) && gridIndex + 1 < n) ++gridIndex;
 if ((down & pad::Up) && gridIndex >= size_t(cols)) gridIndex -= cols;
 if (down & pad::Down) { if (gridIndex + cols < n) gridIndex += cols; else if (gridIndex / cols < (n - 1) / cols) gridIndex = n - 1; }
 if (down & pad::Y) toggleFavorite(view[gridIndex]);
 if (down & pad::A) launch(view[gridIndex], false);
}

int LibraryUI::settingRows() const {
 switch (section) {
 case SecLibrary: return 1;
 case SecGameplay: return 9;
 case SecThemes: return 5;
 case SecUpdates: return 1;
 default: return 0;
 }
}

void LibraryUI::moveSettings(uint32_t down) {
 if (!sectionFocus) {
  if (down & pad::Up) section = (section + SectionCount - 1) % SectionCount;
  if (down & pad::Down) section = (section + 1) % SectionCount;
  if (down & pad::B) { tab = TabHome; return; }
  if (down & (pad::A | pad::Right)) {
   if (section == SecControls || settingRows() > 0) { sectionFocus = true; settingRow = 0; }
   if (section == SecThemes) {
    themeCard = 0;
    for (size_t i = 0; i < ui::presetThemes().size(); ++i) if (ui::presetThemes()[i].id == settings.theme) themeCard = int(i);
   }
  }
  if (section == SecUpdates && (down & (pad::A | pad::Right | pad::Up | pad::Down)) && !updateRequested) { updateRequested = true; pending.type = Command::Update; pending.path = "check"; }
  return;
 }
 int rows = settingRows();
 if (down & pad::B) { sectionFocus = false; return; }
 if (rows <= 0) return;
 if (down & pad::Up) settingRow = (settingRow + rows - 1) % rows;
 if (down & pad::Down) settingRow = (settingRow + 1) % rows;
 if (down & pad::Left) { if (section == SecUpdates) sectionFocus = false; else adjustSetting(-1); }
 if (down & pad::Right) adjustSetting(1);
 if (down & pad::A) activateSetting();
 if ((down & pad::X) && section == SecThemes) {
  if (saveCustomTheme(root, settings)) toast("Saved as My theme"); else toast("Could not save the theme to the SD card");
 }
}

void LibraryUI::adjustSetting(int d) {
 auto cycle = [d](int v, int n) { return (v + d + n) % n; };
 auto& g = settings.gameplay;
 if (section == SecLibrary) { settings.sort = cycle(settings.sort, 4); rebuild(); }
 else if (section == SecGameplay) {
  switch (settingRow) {
  case 0: g.ffSpeed = cycle(g.ffSpeed, 4); break;
  case 1: g.ffToggle = !g.ffToggle; break;
  case 2: g.rewind = !g.rewind; break;
  case 3: g.screen = cycle(g.screen, 3); break;
  case 4: g.look = cycle(g.look, 4); break;
  case 5: g.muteFastForward = !g.muteFastForward; break;
  case 6: g.volume = cycle(g.volume, 4); break;
  case 7: g.boost = !g.boost; break;
  case 8: g.dsThreaded = !g.dsThreaded; break;
  }
 } else if (section == SecThemes) {
  bool custom = fs::exists(root + "themes/my-theme.theme");
  int cards = int(ui::presetThemes().size()) + (custom ? 1 : 0);
  switch (settingRow) {
  case 0:
   themeCard = cycle(themeCard, cards);
   if (themeCard < int(ui::presetThemes().size())) { settings.theme = ui::presetThemes()[themeCard].id; settings.accent.clear(); }
   else loadCustomTheme(root, settings);
   break;
  case 1: {
   int i = int(std::find(accentChoices.begin(), accentChoices.end(), settings.accent) - accentChoices.begin());
   if (i >= int(accentChoices.size())) i = 0;
   settings.accent = accentChoices[cycle(i, int(accentChoices.size()))];
   break;
  }
  case 2: {
   bool want = !settings.wallpaper;
   if (want) { Art test = loadImage(wallpaperPath(root), 16); if (!test.texture) { toast("Add wallpaper.png to /switch/ai1emu/themes/ first"); return; } SDL_DestroyTexture(test.texture); }
   settings.wallpaper = want;
   break;
  }
  case 3: settings.font = cycle(settings.font, ui::FontFamilyCount); break;
  case 4: settings.corners = cycle(settings.corners, 3); break;
  }
  applyTheme();
 }
 persistSettings();
}

void LibraryUI::activateSetting() {
 if (section == SecUpdates) { updateRequested = true; pending.type = Command::Update; pending.path = "press"; return; }
 adjustSetting(1);
}

// ------------------------------------------------------------------ touch
void LibraryUI::onHit(int x, int y, int w, int h, std::function<void()> action) { hits.push_back({{x, y, w, h}, std::move(action)}); }

LibraryUI::Command LibraryUI::handlePointer(const SDL_Event& e) {
 pending = {};
 float x = 0, y = 0;
 bool press = false, release = false, motion = false;
 if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) {
  if (pointerDown && e.tfinger.fingerId != finger) return pending;
  x = e.tfinger.x * 1280.f; y = e.tfinger.y * 720.f;
  press = e.type == SDL_FINGERDOWN; release = e.type == SDL_FINGERUP; motion = e.type == SDL_FINGERMOTION;
  finger = e.tfinger.fingerId;
 } else if (e.type == SDL_MOUSEMOTION) {
  if (e.motion.which == SDL_TOUCH_MOUSEID) return pending;
  x = float(e.motion.x); y = float(e.motion.y); motion = true;
 } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
  if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) return pending;
  x = float(e.button.x); y = float(e.button.y);
  press = e.type == SDL_MOUSEBUTTONDOWN; release = e.type == SDL_MOUSEBUTTONUP;
 } else if (e.type == SDL_MOUSEWHEEL) {
  if (tab == TabLibrary) { gridScroll = std::max(0.f, gridScroll - e.wheel.y * 60.f); gridFollow = false; gridVelocity = 0; }
  return pending;
 } else return pending;

 Uint64 now = SDL_GetTicks64();
 if (press) {
  pointerDown = true; dragging = false; pressX = x; pressY = y;
  dragZone = tab == TabLibrary && x < 890 && y > 160 && y < 650 ? 1 : tab == TabHome && y > 470 && y < 650 ? 2 : 0;
  dragStart = dragZone == 1 ? gridScroll : shelfScroll;
  // Touching a gliding list stops it, like a phone.
  if (dragZone == 1) gridVelocity = 0;
  if (dragZone == 2) shelfVelocity = 0;
  dragVelocity = 0; lastDragPos = dragZone == 1 ? y : x; lastDragTime = now;
  return pending;
 }
 if (motion && pointerDown && dragZone) {
  float dx = x - pressX, dy = y - pressY;
  if (!dragging && ((dragZone == 1 && std::fabs(dy) > 8) || (dragZone == 2 && std::fabs(dx) > 8))) {
   dragging = true;
   // Start from the current finger position so the list does not jump by the threshold.
   if (dragZone == 1) { dragStart = gridScroll + dy; gridFollow = false; } else { dragStart = shelfScroll + dx; shelfFollow = false; }
  }
  if (dragging) {
   float pos = dragZone == 1 ? y : x;
   if (dragZone == 1) gridScroll = std::max(0.f, dragStart - dy);
   else shelfScroll = std::max(0.f, dragStart - dx);
   float dt = float(now - lastDragTime) / 1000.f;
   if (dt > 0.001f) { float instant = -(pos - lastDragPos) / dt; dragVelocity = dragVelocity * .3f + instant * .7f; }
   lastDragPos = pos; lastDragTime = now;
  }
  return pending;
 }
 if (!release || !pointerDown) return pending;
 pointerDown = false;
 if (dragging) {
  dragging = false;
  // Fling: keep moving with the finger's speed (ignored if the finger had stopped).
  float v = now - lastDragTime > 80 ? 0.f : std::clamp(dragVelocity, -4000.f, 4000.f);
  if (dragZone == 1) gridVelocity = v; else shelfVelocity = v;
  return pending;
 }
 if (std::fabs(x - pressX) > 25 || std::fabs(y - pressY) > 25) return pending;
 for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
  SDL_Point p{int(x), int(y)};
  if (SDL_PointInRect(&p, &it->rect)) { auto action = it->action; action(); break; }
 }
 return pending;
}

// ------------------------------------------------------------------ drawing: shared
void LibraryUI::draw(bool present) {
 ++tick;
 hits.clear();
 Uint64 nowTicks = SDL_GetPerformanceCounter();
 frameDt = frameTicks ? std::clamp(float(double(nowTicks - frameTicks) / double(SDL_GetPerformanceFrequency())), 0.f, .05f) : 1.f / 60;
 frameTicks = nowTicks;
 pumpArt();
 auto& t = paint.theme;
 SDL_SetRenderDrawColor(r, t.bg.r, t.bg.g, t.bg.b, 255);
 SDL_RenderClear(r);
 drawBackground();
 drawTop();
 if (tab == TabHome) drawHome();
 else if (tab == TabLibrary) drawLibrary();
 else drawSettings();
 drawToast();
 if (present) SDL_RenderPresent(r);
}

void LibraryUI::drawBackground() {
 if (!settings.wallpaper) return;
 if (!wallpaperTried) { wallpaperTried = true; wallpaper = loadImage(wallpaperPath(root), 1280); }
 if (!wallpaper.texture) return;
 paint.picture(wallpaper.texture, wallpaper.w, wallpaper.h, 0, 0, 1280, 720, 0, paint.theme.bg, false);
 paint.fill(0, 0, 1280, 720, ui::alpha(paint.theme.bg, 150));
}

void LibraryUI::drawTop() {
 auto& p = paint;
 auto& t = p.theme;
 p.text("AI1 Emu", 48, 33, 23, t.text, ui::Heavy);
 static const char* names[tabCount] = {"Home", "Library", "Settings"};
 static const Icon icons[tabCount] = {Icon::Home, Icon::Grid, Icon::Settings};
 int widths[tabCount], total = 0;
 for (int i = 0; i < tabCount; ++i) { widths[i] = 22 + 9 + p.measure(names[i], 18, ui::Bold); total += widths[i]; }
 total += 40 * (tabCount - 1);
 int x = 640 - total / 2;
 p.button("L", x - 50, 48, t.surf2, t.muted);
 for (int i = 0; i < tabCount; ++i) {
  bool on = tab == i;
  SDL_Color ink = on ? t.text : t.muted;
  p.icon(icons[i], x + 11, 48, 22, ink);
  p.text(names[i], x + 31, 37, 18, ink, ui::Bold);
  if (on) p.round(x, 68, widths[i], 4, 2, t.acc);
  int tabId = i;
  onHit(x - 12, 26, widths[i] + 24, 50, [this, tabId] { tab = tabId; });
  x += widths[i] + 40;
 }
 p.button("R", x - 10, 48, t.surf2, t.muted);
 int sx = 1232;
 if (battery >= 0) {
  sx -= 34;
  p.round(sx, 41, 30, 15, 4, t.text);
  p.round(sx + 2, 43, 26, 11, 3, t.bg);
  p.round(sx + 4, 45, std::max(2, 22 * battery / 100), 7, 2, battery <= 15 && !charging ? ui::rgba(0xD9463B) : t.text);
  p.round(sx + 30, 45, 3, 7, 1, t.text);
  sx -= 14;
 }
 std::string clock = clockNow();
 int cw = p.measure(clock, 18, ui::Bold);
 sx -= cw;
 p.text(clock, sx, 37, 18, t.text, ui::Bold);
}

void LibraryUI::drawFooter(const std::vector<std::pair<std::string, std::string>>& hints, const std::vector<uint32_t>& bits) {
 auto& p = paint;
 auto& t = p.theme;
 p.fill(0, 655, 1280, 2, t.line);
 p.text(std::string("v") + AI1EMU_VERSION, 48, 678, 14, t.muted, ui::Bold);
 int x = 1232;
 for (int i = int(hints.size()) - 1; i >= 0; --i) {
  int lw = p.measure(hints[i].second, 16, ui::Bold);
  x -= lw;
  p.text(hints[i].second, x, 678, 16, t.text, ui::Bold);
  int bw = hints[i].first.size() > 1 ? std::max(30, p.measure(hints[i].first, 14, ui::Heavy) + 16) : 30;
  x -= bw + 9;
  bool primary = i == 0 && hints[i].first == "A";
  p.button(hints[i].first, x, 688, primary ? t.acc : t.btn, primary ? t.accText : t.btnText);
  uint32_t bit = i < int(bits.size()) ? bits[i] : 0;
  if (bit) onHit(x - 6, 660, bw + lw + 21, 60, [this, bit] { pending = update(bit, 0); });
  x -= 28;
 }
}

// Shown the moment a game is chosen, before the ROM is read or the emulator starts, so the
// screen never looks frozen. The frame stays up until the game draws or the app hands over.
void LibraryUI::showLaunching(const std::string& game) {
 pumpArt(20);
 draw(false);
 auto& p = paint;
 const auto& t = p.theme;
 SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
 p.fill(0, 0, 1280, 720, {0, 0, 0, 150});
 int w = 560, h = 196, x = 640 - w / 2, y = 262;
 p.shadow(x, y, w, h, p.radius(22), 3);
 p.round(x, y, w, h, p.radius(22), t.surf);
 drawCover(game, x + 28, y + 28, 140, 140, p.radius(14), t.surf, false);
 int tx = x + 196, tw = w - 196 - 28;
 p.text("Starting...", tx, y + 34, 16, t.acc, ui::Bold, tw);
 int lines = p.paragraph(ui::prettyTitle(gameTitle(game)), tx, y + 60, 23, t.text, tw, 28, 2, ui::Heavy);
 p.text(platformInfo(detectPlatform(game)).name, tx, y + 64 + lines * 28, 15, t.muted, ui::Medium, tw);
 p.text("This can take a few seconds", tx, y + h - 40, 14, t.muted, ui::Medium, tw);
 SDL_RenderPresent(r);
}

void LibraryUI::drawToast() {
 if (SDL_GetTicks64() >= toastUntil || toastText.empty()) return;
 auto& p = paint;
 int w = p.measure(toastText, 16, ui::Bold) + 48;
 int x = 640 - w / 2;
 p.shadow(x, 596, w, 44, 12, 2);
 p.round(x, 596, w, 44, p.radius(12), p.theme.text);
 p.text(toastText, x, 608, 16, p.theme.bg, ui::Bold, w, ui::AlignCenter);
}

void LibraryUI::drawCover(const std::string& game, int x, int y, int w, int h, int rad, SDL_Color under, bool showStar) {
 auto& p = paint;
 auto& a = art(coverFor(game));
 if (a.texture) p.picture(a.texture, a.w, a.h, x, y, w, h, rad, under);
 else if (a.state == 0) p.round(x, y, w, h, rad, p.theme.surf2); // still loading: quiet placeholder
 else {
  Platform pl = detectPlatform(game);
  SDL_Color c = ui::consoleColor(int(pl));
  p.round(x, y, w, h, rad, c);
  p.round(x, y + h / 2, w, h / 2, rad, ui::mix(c, {0, 0, 0, 255}, .18f));
  p.fill(x, y + h / 2, w, std::min(rad, h / 2), ui::mix(c, {0, 0, 0, 255}, .18f));
  int size = std::max(14, h / 3);
  p.text(ui::initials(gameTitle(game)), x, y + h / 2 - size * 2 / 3, size, {255, 255, 255, 240}, ui::Heavy, w, ui::AlignCenter);
  if (h >= 90) p.text(platformInfo(pl).shortName, x, y + h - 30, 13, {255, 255, 255, 220}, ui::Bold, w, ui::AlignCenter);
 }
 if (showStar && favorites.count(game)) {
  int s = std::max(22, w / 6);
  p.circle(x + w - s / 2 - 7, y + s / 2 + 7, s / 2, {255, 255, 255, 240});
  p.icon(Icon::Star, x + w - s / 2 - 7, y + s / 2 + 7, s * 2 / 3, ui::rgba(0xF2A900));
 }
}

void LibraryUI::segment(int x, int y, int rightEdge, const std::vector<std::string>& labels, int selected, bool focused, const std::function<void(int)>& onTap) {
 auto& p = paint;
 auto& t = p.theme;
 std::vector<int> widths;
 int total = 8;
 for (auto& l : labels) { widths.push_back(p.measure(l, 15, ui::Bold) + 30); total += widths.back(); }
 int sx = rightEdge - total;
 (void)x;
 p.round(sx, y, total, 40, 20, t.surf2);
 int cx = sx + 4;
 for (size_t i = 0; i < labels.size(); ++i) {
  bool on = int(i) == selected;
  if (on) p.round(cx, y + 4, widths[i], 32, 16, focused ? t.acc : t.surf);
  p.text(labels[i], cx, y + 12, 15, on ? (focused ? t.accText : t.text) : t.muted, ui::Bold, widths[i], ui::AlignCenter);
  int index = int(i);
  if (onTap) onHit(cx, y, widths[i], 40, [onTap, index] { onTap(index); });
  cx += widths[i];
 }
}

void LibraryUI::toggle(int x, int y, bool on) {
 paint.round(x, y, 56, 32, 16, on ? paint.theme.acc : paint.theme.surf2);
 paint.circle(on ? x + 40 : x + 16, y + 16, 12, {255, 255, 255, 255});
}

// ------------------------------------------------------------------ Home
void LibraryUI::drawHome() {
 auto& p = paint;
 auto& t = p.theme;
 if (all.empty()) {
  p.shadow(48, 104, 1184, 520, p.radius(18), 2);
  p.round(48, 104, 1184, 520, p.radius(18), t.surf);
  p.circle(640, 260, 56, t.surf2);
  p.icon(Icon::Grid, 640, 260, 52, t.acc);
  p.text("Your game library starts here", 48, 344, 30, t.text, ui::Heavy, 1184, ui::AlignCenter);
  p.text("Put your own game files in /switch/ai1emu/roms/<console>/ on the SD card,", 48, 396, 18, t.muted, ui::Medium, 1184, ui::AlignCenter);
  p.text("for example roms/gba/ or roms/snes/. Then open AI1 Emu again.", 48, 424, 18, t.muted, ui::Medium, 1184, ui::AlignCenter);
  p.text("Covers are optional. Everything works offline.", 48, 470, 16, t.muted, ui::Medium, 1184, ui::AlignCenter);
  drawFooter({{"+", "Settings"}, {"B", "Close"}}, {pad::Plus, pad::B});
  return;
 }
 std::string hero = !recent.empty() ? recent[0] : sorted[0];
 bool played = !recent.empty();
 bool autoSave = hasAutoSave(hero);
 int heroButtons = autoSave ? 2 : 1;
 if (homeZone == 0) homeIndex = std::min(homeIndex, heroButtons - 1);
 int rad = p.radius(18);
 // hero
 p.shadow(48, 104, 780, 318, rad, 2);
 p.round(48, 104, 780, 318, rad, t.surf);
 drawCover(hero, 76, 132, 262, 262, p.radius(10), t.surf, true);
 onHit(76, 132, 262, 262, [this, hero] { launch(hero, true); });
 int tx = 372;
 auto record = history.count(hero) ? history[hero] : PlayRecord{};
 p.text(played ? "Last played " + lower(formatWhen(record.lastPlayed, std::time(nullptr))) : "Ready to play", tx, 152, 16, t.muted, ui::Bold, 430);
 std::string title = ui::prettyTitle(gameTitle(hero));
 int lines = p.paragraph(title, tx, 178, 36, t.text, 430, 42, 2, ui::Heavy);
 int my = 178 + lines * 42 + 6;
 std::string console = platformInfo(detectPlatform(hero)).name;
 p.text(console, tx, my, 16, t.muted, ui::Medium, 230);
 if (record.seconds > 0) {
  int cwid = p.measure(console, 16, ui::Medium);
  p.icon(Icon::Clock, tx + cwid + 30, my + 9, 19, t.muted);
  p.text(formatPlaytime(record.seconds), tx + cwid + 45, my, 16, t.muted, ui::Medium, 200);
 }
 int by = 338;
 auto heroButton = [&](int index, int x, const std::string& label, bool primary) {
  int w = p.measure(label, 19, ui::Heavy) + (primary ? 84 : 60);
  bool focus = homeZone == 0 && homeIndex == index;
  if (focus) p.cursor(x, by, w, 56, 28, t.surf);
  p.round(x, by, w, 56, 28, primary ? t.acc : t.surf2);
  if (primary) p.icon(Icon::Play, x + 34, by + 28, 22, t.accText);
  p.text(label, x + (primary ? 54 : 30), by + 17, 19, primary ? t.accText : t.text, ui::Heavy);
  onHit(x, by, w, 56, [this, hero, index] { launch(hero, index == 0); });
  return w;
 };
 int w0 = heroButton(0, tx, autoSave ? "Resume" : "Play", true);
 if (autoSave) heroButton(1, tx + w0 + 14, "Start over", false);
 // recently played
 p.shadow(852, 104, 380, 318, rad, 2);
 p.round(852, 104, 380, 318, rad, t.surf);
 p.text("Recently played", 872, 126, 16, t.muted, ui::Bold);
 if (recent.size() <= 1) {
  p.paragraph(recent.empty() ? "Games you play will show up here, so you can jump back in." : "Play another game and it will show up here.", 872, 170, 16, t.muted, 336, 24, 4);
 }
 for (size_t i = 1; i < recent.size() && i <= 3; ++i) {
  int ry = 152 + int(i - 1) * 86;
  bool focus = homeZone == 1 && homeIndex == int(i - 1);
  if (i > 1) p.fill(872, ry - 4, 340, 2, t.line);
  if (focus) { p.cursor(864, ry + 2, 356, 76, p.radius(12), t.surf); p.round(864, ry + 2, 356, 76, p.radius(12), t.surf); }
  drawCover(recent[i], 874, ry + 9, 62, 62, p.radius(8), t.surf, false);
  p.text(ui::prettyTitle(gameTitle(recent[i])), 950, ry + 16, 17, t.text, ui::Bold, 260);
  auto rec = history[recent[i]];
  p.text(formatWhen(rec.lastPlayed, std::time(nullptr)) + " \xC2\xB7 " + formatPlaytime(rec.seconds, true), 950, ry + 42, 15, t.muted, ui::Medium, 260);
  std::string g = recent[i];
  onHit(864, ry, 356, 80, [this, g] { launch(g, true); });
 }
 // shelf
 p.text("All games", 48, 446, 21, t.text, ui::Heavy);
 int hw = p.measure("All games", 21, ui::Heavy);
 p.text(std::to_string(all.size()) + (all.size() == 1 ? " game" : " games") + " \xC2\xB7 " + lower(sortName(settings.sort)), 48 + hw + 14, 450, 16, t.muted, ui::Medium);
 float target = std::max(0.f, shelfIndex * 166.f - 166.f * 3);
 float maxScroll = std::max(0.f, sorted.size() * 166.f - 1184.f);
 target = std::min(target, maxScroll);
 bool shelfHeld = pointerDown && dragZone == 2 && dragging;
 if (shelfFollow) shelfScroll += (target - shelfScroll) * (1.f - std::exp(-frameDt * 16.f));
 else if (!shelfHeld) {
  shelfScroll += shelfVelocity * frameDt;
  shelfVelocity *= std::exp(-frameDt * 3.5f);
  if (std::fabs(shelfVelocity) < 8) shelfVelocity = 0;
 }
 if (shelfScroll < 0 || shelfScroll > maxScroll) shelfVelocity = 0;
 shelfScroll = std::clamp(shelfScroll, 0.f, maxScroll);
 SDL_Rect clip{36, 470, 1220, 180};
 SDL_RenderSetClipRect(r, &clip);
 for (size_t i = 0; i < sorted.size(); ++i) {
  int x = 48 + int(i * 166 - shelfScroll);
  if (x < -170 || x > 1290) continue;
  bool focus = homeZone == 2 && shelfIndex == i;
  if (focus) p.cursor(x, 484, 148, 148, p.radius(10), t.bg);
  else p.shadow(x, 484, 148, 148, p.radius(10), 1);
  drawCover(sorted[i], x, 484, 148, 148, p.radius(10), focus ? t.bg : t.bg, true);
  size_t index = i;
  onHit(x, 484, 148, 148, [this, index] { if (homeZone == 2 && shelfIndex == index) launch(sorted[index], false); else { homeZone = 2; shelfIndex = index; } });
 }
 SDL_RenderSetClipRect(r, nullptr);
 bool fav = favorites.count(selectedGame()) > 0;
 std::string a = homeZone == 0 ? (homeIndex == 0 && autoSave ? "Resume" : homeIndex == 1 ? "Start over" : "Play") : homeZone == 1 ? "Resume" : "Play";
 drawFooter({{"A", a}, {"Y", fav ? "Unfavourite" : "Favourite"}, {"X", "Search"}, {"+", "Settings"}}, {pad::A, pad::Y, pad::X, pad::Plus});
}

// ------------------------------------------------------------------ Library
void LibraryUI::drawLibrary() {
 auto& p = paint;
 auto& t = p.theme;
 // search
 p.shadow(48, 100, 240, 46, 23, 1);
 p.round(48, 100, 240, 46, 23, t.surf);
 p.icon(Icon::Search, 74, 123, 20, t.muted);
 p.text(query.empty() ? "Search" : query, 94, 112, 16, query.empty() ? t.muted : t.text, ui::Bold, 180);
 onHit(48, 100, 240, 46, [this] { pending.type = Command::Search; });
 // Console filters: All, Favourites (star) and every console, so each one (3DS
 // included) is visible without scrolling. Chips shrink to fit; only if they
 // still cannot fit does the bar scroll with an arrow.
 std::vector<std::string> labels = {"All", ""};
 for (int c : consoles) labels.push_back(platformInfo(Platform(c)).shortName);
 const int barX = 302, barRight = 1232, area = barRight - barX;
 int fontSize = 15, pad = 28;
 std::vector<int> widths;
 auto measureAll = [&] {
  widths.clear();
  int total = 8;
  for (size_t i = 0; i < labels.size(); ++i) {
   int w = i == 1 ? 40 : p.measure(labels[i], fontSize, ui::Bold) + pad + (i >= 2 ? 14 : 0);
   widths.push_back(w);
   total += w;
  }
  return total;
 };
 while (measureAll() > area && fontSize > 12) { --fontSize; pad = std::max(16, pad - 4); }
 int first = 0;
 auto span = [&](int from, int to) { int s = 8; for (int i = from; i <= to; ++i) s += widths[size_t(i)]; return s; };
 bool scrolls = span(0, int(labels.size()) - 1) > area;
 int usable = scrolls ? area - 40 : area;
 while (first < filter && span(first, filter) > usable) ++first;
 int cx = barX + 4, last = first;
 while (last + 1 < int(labels.size()) && span(first, last + 1) <= usable) ++last;
 p.round(barX, 103, span(first, last), 40, 20, t.surf2);
 chipRects.assign(labels.size(), SDL_Rect{0, 0, 0, 0});
 for (int i = first; i <= last; ++i) {
  bool on = filter == i;
  int w = widths[size_t(i)];
  if (on) p.round(cx, 107, w, 32, 16, t.surf);
  int lx = cx + pad / 2;
  if (i == 1) p.icon(Icon::Star, cx + w / 2, 123, 18, on ? ui::rgba(0xF2A900) : t.muted);
  if (i >= 2) { p.circle(lx + 4, 123, 4, ui::consoleColor(consoles[size_t(i - 2)])); lx += 14; }
  if (i != 1) p.text(labels[size_t(i)], lx, 123 - fontSize * 6 / 10, fontSize, on ? t.text : t.muted, ui::Bold);
  int index = i;
  chipRects[size_t(i)] = {cx, 103, w, 40};
  onHit(cx, 103, w, 40, [this, index] { filter = index; gridIndex = 0; gridScroll = 0; rebuild(); });
  cx += w;
 }
 if (scrolls) {
  // Fallback for very narrow fonts/settings: touch can still reach every console.
  p.icon(Icon::Right, barRight - 18, 123, 18, t.muted);
  onHit(barRight - 32, 101, 32, 44, [this] { filter = (filter + 1) % (2 + int(consoles.size())); gridIndex = 0; gridScroll = 0; rebuild(); });
 }
 // grid
 const int cols = 5, gx = 48, gy = 170, cell = 151, gap = 18, rowH = cell + 62;
 if (view.empty()) {
  bool consoleEmpty = query.empty() && filter >= 2 && filter - 2 < int(consoles.size());
  if (consoleEmpty) {
   const auto& info = platformInfo(Platform(consoles[size_t(filter - 2)]));
   Platform pl = info.id;
   p.circle(gx + 414, 250, 44, ui::consoleColor(int(pl)));
   p.text(info.shortName, gx + 370, 239, 18, {255, 255, 255, 255}, ui::Heavy, 88, ui::AlignCenter);
   p.text(std::string("No ") + info.shortName + " games yet", gx, 312, 22, t.text, ui::Heavy, 828, ui::AlignCenter);
   p.text(std::string("Copy your game files to /switch/ai1emu/roms/") + info.slug + "/ on the SD card,", gx, 350, 16, t.muted, ui::Medium, 828, ui::AlignCenter);
   p.text("then open AI1 Emu again.", gx, 374, 16, t.muted, ui::Medium, 828, ui::AlignCenter);
   std::string formats = pl == Platform::N3DS ? "3DS: decrypted .3ds, .cci, .cxi, or compressed .zcci / .zcxi. CIA files are not supported."
                       : pl == Platform::N64 ? "N64: .z64, .v64 or .n64" : pl == Platform::DS ? "DS: .nds" : pl == Platform::DSi ? "DSi: .dsi, or .nds in roms/dsi/" : "";
   if (!formats.empty()) p.text(formats, gx, 414, 15, t.muted, ui::Bold, 828, ui::AlignCenter);
  } else {
   p.text(query.empty() ? (filter == 1 ? "No favourites yet" : "No games here") : "No games match \"" + query + "\"", gx, 300, 22, t.text, ui::Heavy, 828, ui::AlignCenter);
   p.text(filter == 1 ? "Press Y on a game to add it to your favourites." : "Try another console or search.", gx, 336, 16, t.muted, ui::Medium, 828, ui::AlignCenter);
  }
 } else {
  int rowOf = int(gridIndex) / cols;
  float target = std::max(0.f, (rowOf - 0.6f) * rowH);
  float maxScroll = std::max(0.f, float(((int(view.size()) + cols - 1) / cols) * rowH - 470));
  target = std::min(target, maxScroll);
  bool gridHeld = pointerDown && dragZone == 1 && dragging;
  if (gridFollow) gridScroll += (target - gridScroll) * (1.f - std::exp(-frameDt * 16.f));
  else if (!gridHeld) {
   gridScroll += gridVelocity * frameDt;
   gridVelocity *= std::exp(-frameDt * 3.5f);
   if (std::fabs(gridVelocity) < 8) gridVelocity = 0;
  }
  if (gridScroll < 0 || gridScroll > maxScroll) gridVelocity = 0;
  gridScroll = std::clamp(gridScroll, 0.f, maxScroll);
  SDL_Rect clip{gx - 12, gy - 12, 828 + 24, 486};
  SDL_RenderSetClipRect(r, &clip);
  for (size_t i = 0; i < view.size(); ++i) {
   int x = gx + int(i % cols) * (cell + gap), y = gy + int(i / cols) * rowH - int(gridScroll);
   if (y < gy - rowH || y > 660) continue;
   bool focus = gridIndex == i;
   if (focus) p.cursor(x, y, cell, cell, p.radius(10), t.bg);
   else p.shadow(x, y, cell, cell, p.radius(10), 1);
   drawCover(view[i], x, y, cell, cell, p.radius(10), t.bg, true);
   p.text(ui::prettyTitle(gameTitle(view[i])), x, y + cell + 9, 15, t.text, ui::Bold, cell);
   long long secs = history.count(view[i]) ? history[view[i]].seconds : 0;
   p.text(secs > 0 ? formatPlaytime(secs, true) : "New", x, y + cell + 31, 13, t.muted, ui::Medium, cell);
   size_t index = i;
   onHit(x, std::max(y, gy - 12), cell, std::min(cell + 50, 650 - y), [this, index] { if (gridIndex == index) launch(view[index], false); else gridIndex = index; });
  }
  SDL_RenderSetClipRect(r, nullptr);
 }
 // details
 int sx = 904, sy = 170, sw2 = 328, sh = 470, rad = p.radius(18);
 p.shadow(sx, sy, sw2, sh, rad, 2);
 p.round(sx, sy, sw2, sh, rad, t.surf);
 if (gridIndex < view.size()) {
  std::string g = view[gridIndex];
  drawCover(g, sx + 22, sy + 22, sw2 - 44, 116, p.radius(10), t.surf, false);
  int lines = p.paragraph(ui::prettyTitle(gameTitle(g)), sx + 22, sy + 152, 21, t.text, sw2 - 44, 26, 2, ui::Heavy);
  int ky = sy + 152 + lines * 26 + 2;
  {
   auto from = gamePatchedFrom(g);
   std::string sub = platformInfo(detectPlatform(g)).name;
   if (!from.empty()) sub = "ROM hack of " + shortTitle(from);
   p.text(sub, sx + 22, ky, 15, t.muted, ui::Medium, sw2 - 44);
  }
  auto rec = history.count(g) ? history[g] : PlayRecord{};
  auto& info = infoFor(g);
  std::vector<std::pair<std::string, std::string>> kv = {
   {"Play time", rec.seconds > 0 ? formatPlaytime(rec.seconds) : "None yet"},
   {"Last played", formatWhen(rec.lastPlayed, std::time(nullptr))},
   {"Save states", std::to_string(info.states)},
   {"Cheats", info.cheatsOn ? std::to_string(info.cheatsOn) + " on" : "Off"}};
  int ry = ky + 28;
  p.fill(sx + 22, ry, sw2 - 44, 2, t.line);
  for (auto& row : kv) {
   p.text(row.first, sx + 22, ry + 9, 15, t.muted, ui::Medium, 150);
   p.text(row.second, sx + 172, ry + 9, 15, t.text, ui::Bold, sw2 - 194, ui::AlignRight);
   ry += 35;
   p.fill(sx + 22, ry, sw2 - 44, 2, t.line);
  }
  int bw = sw2 - 44, by = sy + sh - 74;
  p.round(sx + 22, by, bw, 52, 26, t.acc);
  int lw = p.measure("Play", 19, ui::Heavy);
  p.icon(Icon::Play, sx + 22 + bw / 2 - lw / 2 - 12, by + 26, 22, t.accText);
  p.text("Play", sx + 22 + bw / 2 - lw / 2 + 12, by + 15, 19, t.accText, ui::Heavy);
  onHit(sx + 22, by, bw, 52, [this, g] { launch(g, false); });
 }
 bool fav = gridIndex < view.size() && favorites.count(view[gridIndex]);
 static const char* shortSort[] = {"Recent", "A\xE2\x80\x93Z", "Z\xE2\x80\x93" "A", "Most played"};
 drawFooter({{"A", "Play"}, {"Y", fav ? "Unfavourite" : "Favourite"}, {"X", "Search"}, {"ZL", "Console"}, {"ZR", "Console"}, {"-", std::string("Sort: ") + shortSort[std::clamp(settings.sort, 0, 3)]}},
            {pad::A, pad::Y, pad::X, pad::ZL, pad::ZR, pad::Minus});
}

// ------------------------------------------------------------------ Settings
void LibraryUI::drawSettings() {
 auto& p = paint;
 auto& t = p.theme;
 int rad = p.radius(18);
 p.shadow(48, 104, 262, 530, rad, 2);
 p.round(48, 104, 262, 530, rad, t.surf);
 for (int i = 0; i < SectionCount; ++i) {
  int y = 116 + i * 52;
  bool on = section == i;
  if (on && !sectionFocus) { p.cursor(58, y, 242, 46, p.radius(10), t.surf); p.round(58, y, 242, 46, p.radius(10), t.surf); }
  else if (on) p.round(58, y, 242, 46, p.radius(10), t.surf2);
  p.icon(sectionIcon(i), 84, y + 23, 22, on ? t.acc : t.muted);
  p.text(sectionName(i), 108, y + 13, 17, t.text, ui::Bold, 180);
  int index = i;
  onHit(58, y, 242, 46, [this, index] {
   section = index; sectionFocus = settingRows() > 0 || index == SecControls; settingRow = 0;
   if (index == SecUpdates && !updateRequested) { updateRequested = true; pending.type = Command::Update; pending.path = "check"; }
  });
 }
 drawSection(334, 104, 898, 530);
 std::vector<std::pair<std::string, std::string>> hints;
 std::vector<uint32_t> bits;
 if (!sectionFocus) { hints = {{"A", "Open"}, {"B", "Back"}}; bits = {pad::A, pad::B}; }
 else if (section == SecControls) { hints = {{"B", "Back"}}; bits = {pad::B}; }
 else if (section == SecUpdates) { hints = {{"A", updateButton}, {"B", "Back"}}; bits = {pad::A, pad::B}; }
 else if (section == SecThemes) { hints = {{"A", "Change"}, {"X", "Save my theme"}, {"B", "Back"}}; bits = {pad::A, pad::X, pad::B}; }
 else { hints = {{"A", "Change"}, {"B", "Back"}}; bits = {pad::A, pad::B}; }
 drawFooter(hints, bits);
}

void LibraryUI::drawSection(int x, int y, int w, int h) {
 auto& p = paint;
 auto& t = p.theme;
 if (section == SecControls) { drawControls(x, y, w, h); return; }
 if (section == SecThemes) { drawThemes(x, y, w, h); return; }
 if (section == SecMap) { drawMap(x, y, w, h); return; }
 if (section == SecAbout) { drawAbout(x, y, w, h); return; }
 struct RowDef { std::string label, sub; int kind; std::vector<std::string> options; int value; };
 std::vector<RowDef> rows;
 std::string heading = sectionName(section), sub;
 auto& g = settings.gameplay;
 if (section == SecLibrary) {
  sub = "How your games are listed.";
  rows.push_back({"Sort order", "Used on Home and in the Library", 0, {"Recent", "A\xE2\x80\x93Z", "Z\xE2\x80\x93" "A", "Most played"}, settings.sort});
 } else if (section == SecGameplay) {
  sub = "Defaults for every game. Change one game from its in-game menu.";
  rows.push_back({"Fast forward speed", "Right stick button (R3) while playing", 0, {"1.5\xC3\x97", "2\xC3\x97", "3\xC3\x97", "4\xC3\x97"}, g.ffSpeed});
  rows.push_back({"Fast forward button", "Hold to speed up, or press to switch on and off", 0, {"Hold", "Toggle"}, g.ffToggle});
  rows.push_back({"Rewind", "Hold the left stick button (L3) to go back", 1, {}, g.rewind});
  rows.push_back({"Screen size", "Pixel perfect keeps every pixel the same size", 0, {"Fit", "Pixel perfect", "Stretch"}, g.screen});
  rows.push_back({"Look", "Add a classic screen effect", 0, {"Sharp", "Smooth", "LCD", "CRT"}, g.look});
  rows.push_back({"Mute while fast forwarding", "Avoids sped-up sound", 1, {}, g.muteFastForward});
  rows.push_back({"Volume", "Game sound level", 0, {"Mute", "50%", "75%", "100%"}, g.volume});
  rows.push_back({"Performance boost", "Faster CPU (3DS uses sys-clk). More battery, runs warmer.", 1, {}, g.boost});
  rows.push_back({"DS 3D rendering", "Two cores is faster in 3D games", 0, {"Two cores", "One core"}, g.dsThreaded ? 0 : 1});
 } else if (section == SecUpdates) {
  sub = "Updates download only when you ask. Games always play offline.";
 }
 p.text(heading, x, y + 2, 26, t.text, ui::Heavy, w);
 p.text(sub, x, y + 40, 15, t.muted, ui::Medium, w);
 int ry = y + 78;
 bool compact = rows.size() > 5;
 int rh = compact ? 56 : 66, gap = compact ? 8 : 12;
 int visible = std::max(1, (y + h - ry) / (rh + gap)), first = 0;
 if (int(rows.size()) > visible) first = std::clamp(sectionFocus ? settingRow - visible + 2 : 0, 0, int(rows.size()) - visible);
 if (first > 0) p.icon(Icon::Up, x + w / 2, y + 66, 18, t.muted);
 if (first + visible < int(rows.size())) p.icon(Icon::Down, x + w / 2, y + h + 8, 18, t.muted);
 for (size_t i = size_t(first); i < rows.size() && int(i) < first + visible; ++i) {
  auto& d = rows[i];
  bool sel = sectionFocus && settingRow == int(i);
  int rr = p.radius(12);
  if (sel) p.cursor(x, ry, w, rh, rr, t.bg);
  else p.shadow(x, ry, w, rh, rr, 1);
  p.round(x, ry, w, rh, rr, t.surf);
  p.text(d.label, x + 20, ry + (compact ? 8 : 12), 17, t.text, ui::Bold, w - 380);
  p.text(d.sub, x + 20, ry + (compact ? 31 : 37), 14, t.muted, ui::Medium, d.kind == 0 ? w - 420 : w - 120);
  int index = int(i);
  if (d.kind == 0) segment(x, ry + (rh - 40) / 2, x + w - 12, d.options, d.value, sel, [this, index](int option) {
   sectionFocus = true; settingRow = index;
   for (int guard = 0; guard < 8; ++guard) {
    int current = 0;
    auto& gg = settings.gameplay;
    if (section == SecLibrary) current = settings.sort;
    else switch (index) { case 0: current = gg.ffSpeed; break; case 1: current = gg.ffToggle; break; case 3: current = gg.screen; break; case 4: current = gg.look; break; case 6: current = gg.volume; break; case 8: current = gg.dsThreaded ? 0 : 1; break; default: break; }
    if (current == option) break;
    adjustSetting(1);
   }
  });
  else { toggle(x + w - 76, ry + (rh - 32) / 2, d.value != 0); onHit(x, ry, w, rh, [this, index] { sectionFocus = true; settingRow = index; adjustSetting(1); }); }
  ry += rh + gap;
 }
 if (section == SecLibrary) {
  int iy = ry + 10;
  p.shadow(x, iy, w, 170, p.radius(12), 1);
  p.round(x, iy, w, 170, p.radius(12), t.surf);
  p.icon(Icon::Folder, x + 36, iy + 38, 26, t.acc);
  p.text("Adding games", x + 64, iy + 26, 17, t.text, ui::Bold, w - 90);
  p.paragraph("Copy your own game files into /switch/ai1emu/roms/<console>/ on the SD card, for example roms/gba/. Covers go in covers/<console>/ with the same name as the game file, or use the AI1 Emu artwork tool on a PC.", x + 64, iy + 54, 15, t.muted, w - 100, 22, 5);
  p.text(std::to_string(all.size()) + " games found \xC2\xB7 " + std::to_string(consoles.size()) + " consoles", x + 64, iy + 136, 14, t.muted, ui::Bold, w - 100);
 }
 if (section == SecUpdates) {
  int by = y + 78;
  bool sel = sectionFocus;
  int rr = p.radius(14);
  if (sel) p.cursor(x, by, w, 110, rr, t.bg); else p.shadow(x, by, w, 110, rr, 1);
  p.round(x, by, w, 110, rr, t.surf);
  p.circle(x + 54, by + 55, 30, updateBusy ? t.surf2 : t.acc);
  p.icon(Icon::Update, x + 54, by + 55, 30, updateBusy ? t.muted : t.accText);
  p.text(updateButton, x + 104, by + 30, 20, t.text, ui::Heavy, w - 130);
  p.text(std::string("You have version ") + AI1EMU_VERSION, x + 104, by + 62, 15, t.muted, ui::Medium, w - 130);
  onHit(x, by, w, 110, [this] { sectionFocus = true; activateSetting(); });
  if (!updateStatus.empty()) p.paragraph(updateStatus, x + 4, by + 136, 16, t.muted, w - 8, 24, 4);
  p.paragraph("Updates replace the AI1 Emu app and its emulators. Your games, saves, states, cheats and themes stay where they are.", x + 4, y + h - 60, 14, t.muted, w - 8, 21, 2);
 }
}

void LibraryUI::drawThemes(int x, int y, int w, int h) {
 auto& p = paint;
 auto& t = p.theme;
 (void)h;
 p.text("Themes", x, y + 2, 26, t.text, ui::Heavy, w);
 p.text("Changes show straight away.", x, y + 40, 15, t.muted, ui::Medium, w);
 bool custom = fs::exists(root + "themes/my-theme.theme");
 auto& presets = ui::presetThemes();
 int cards = int(presets.size()) + (custom ? 1 : 0), gap = 12, cw = (w - gap * (cards - 1)) / cards, cy = y + 78, ch = 118;
 bool rowSel = sectionFocus && settingRow == 0;
 for (int i = 0; i < cards; ++i) {
  int cx = x + i * (cw + gap);
  ui::Theme th = i < int(presets.size()) ? presets[size_t(i)] : resolveTheme(settings);
  bool current = i < int(presets.size()) ? settings.theme == th.id && (settings.accent.empty() || !custom) : false;
  if (i == int(presets.size())) { AppSettings tmp = settings; loadCustomTheme(root, tmp); th = resolveTheme(tmp); th.name = "My theme"; }
  bool focus = rowSel && themeCard == i;
  int rr = p.radius(12);
  if (focus) p.cursor(cx, cy, cw, ch, rr, t.bg); else p.shadow(cx, cy, cw, ch, rr, 1);
  p.round(cx, cy, cw, ch, rr, th.surf);
  p.round(cx, cy, cw, 80, rr, th.bg);
  p.fill(cx, cy + 80 - rr, cw, rr, th.bg);
  p.round(cx + 12, cy + 14, cw * 2 / 5, 10, 5, th.acc);
  p.round(cx + 12, cy + 32, cw - 24, 10, 5, th.surf);
  p.round(cx + 12, cy + 50, cw * 3 / 5, 10, 5, th.surf);
  p.text(th.name, cx + 12, cy + 88, 15, th.text, ui::Bold, cw - 40);
  if (current) p.icon(Icon::Check, cx + cw - 20, cy + 98, 18, th.acc);
  int index = i;
  onHit(cx, cy, cw, ch, [this, index, cards] { sectionFocus = true; settingRow = 0; int steps = (index - themeCard + cards) % cards; if (steps == 0) steps = cards; for (int s = 0; s < steps; ++s) adjustSetting(1); });
 }
 p.text("Make your own", x, cy + ch + 22, 19, t.text, ui::Heavy, w);
 struct Custom { std::string label, sub; };
 const Custom rows[4] = {{"Accent colour", "Buttons and highlights"}, {"Background", "Plain, or your own picture: themes/wallpaper.png"}, {"Font", "Used everywhere in the app"}, {"Corners", "How round cards look"}};
 int ry = cy + ch + 58, colW = (w - 12) / 2, rh = 92;
 for (int i = 0; i < 4; ++i) {
  int rx = x + (i % 2) * (colW + 12), yy = ry + (i / 2) * (rh + 12);
  bool sel = sectionFocus && settingRow == i + 1;
  int rr = p.radius(12);
  if (sel) p.cursor(rx, yy, colW, rh, rr, t.bg); else p.shadow(rx, yy, colW, rh, rr, 1);
  p.round(rx, yy, colW, rh, rr, t.surf);
  p.text(rows[i].label, rx + 18, yy + 12, 17, t.text, ui::Bold, colW - 36);
  p.text(rows[i].sub, rx + 18, yy + 36, 13, t.muted, ui::Medium, colW - 36);
  int index = i + 1;
  if (i == 0) {
   int sx = rx + 18;
   for (size_t k = 0; k < accentChoices.size(); ++k) {
    SDL_Color c = ui::presetTheme(settings.theme == "custom" ? "white" : settings.theme).acc;
    if (!accentChoices[k].empty()) ui::parseHex(accentChoices[k], c);
    bool on = settings.accent == accentChoices[k];
    if (on) p.circle(sx + 13, yy + 72, 16, t.text);
    p.circle(sx + 13, yy + 72, on ? 13 : 13, on ? t.surf : t.line);
    p.circle(sx + 13, yy + 72, 11, c);
    if (k == 0) p.text("A", sx, yy + 64, 11, ui::onColor(c), ui::Heavy, 26, ui::AlignCenter);
    std::string choice = accentChoices[k];
    onHit(sx, yy + 56, 30, 32, [this, choice] { sectionFocus = true; settingRow = 1; settings.accent = choice; applyTheme(); persistSettings(); });
    sx += 34;
   }
  } else {
   std::vector<std::string> opts = i == 1 ? std::vector<std::string>{"Plain", "Picture"} : i == 2 ? std::vector<std::string>{"Rounded", "Rubik", "Nunito"} : std::vector<std::string>{"Square", "Soft", "Round"};
   int value = i == 1 ? settings.wallpaper : i == 2 ? settings.font : settings.corners;
   segment(rx, yy + 46, rx + colW - 14, opts, value, sel, [this, index, value, n = int(opts.size())](int option) {
    sectionFocus = true; settingRow = index;
    int steps = (option - value + n) % n;
    for (int s = 0; s < steps; ++s) adjustSetting(1);
   });
  }
 }
}

// ------------------------------------------------------------------ Controls
void LibraryUI::padButton(int cx, int cy, int radius, const char* label, uint32_t bit, bool accent) {
 auto& t = paint.theme;
 bool on = padHeld & bit;
 if (on) paint.circle(cx, cy, radius + 6, ui::alpha(t.acc, 90));
 paint.circle(cx, cy, radius, on ? t.acc : accent ? t.text : t.btn);
 if (label && *label) paint.text(label, cx - radius, cy - int(radius * .62f), std::max(12, int(radius * 1.05f)), on ? t.accText : t.btnText, ui::Heavy, radius * 2, ui::AlignCenter);
}

void LibraryUI::padPill(int x, int y, int w, int h, const char* label, uint32_t bit) {
 auto& t = paint.theme;
 bool on = padHeld & bit;
 if (on) paint.round(x - 5, y - 5, w + 10, h + 10, (h + 10) / 2, ui::alpha(t.acc, 90));
 paint.round(x, y, w, h, h / 2, on ? t.acc : t.btn);
 paint.text(label, x, y + int(h * .16f), std::max(12, int(h * .66f)), on ? t.accText : t.btnText, ui::Heavy, w, ui::AlignCenter);
}

void LibraryUI::drawControls(int x, int y, int w, int h) {
 auto& p = paint;
 auto& t = p.theme;
 int rr = p.radius(16);
 int lw = 470;
 p.shadow(x, y, lw, h, rr, 1);
 p.round(x, y, lw, h, rr, t.surf);
 p.text("Try any button", x + 24, y + 20, 20, t.text, ui::Heavy, lw - 48);
 const float s = .68f;
 const int ox = x + 10, oy = y + 110;
 auto X = [&](float v) { return ox + int(v * s + .5f); };
 auto Y = [&](float v) { return oy + int(v * s + .5f); };
 auto S = [&](float v) { return int(v * s + .5f); };
 SDL_Color left = t.surf2, right = t.surf2;
 p.round(X(40), Y(42), S(150), S(252), S(72), left); p.fill(X(118), Y(42), S(72), S(252), left);
 p.round(X(490), Y(42), S(150), S(252), S(72), right); p.fill(X(490), Y(42), S(72), S(252), right);
 p.round(X(196), Y(42), S(288), S(252), S(16), t.text);
 p.round(X(210), Y(56), S(260), S(224), S(12), t.bg);
 p.text("AI1 Emu", X(196), Y(150), 18, t.text, ui::Heavy, S(288), ui::AlignCenter);
 padPill(X(62), Y(10), S(62), S(24), "ZL", pad::ZL); padPill(X(130), Y(18), S(54), S(20), "L", pad::L);
 padPill(X(496), Y(18), S(54), S(20), "R", pad::R); padPill(X(556), Y(10), S(62), S(24), "ZR", pad::ZR);
 padButton(X(170), Y(70), S(13), "-", pad::Minus); padButton(X(510), Y(70), S(13), "+", pad::Plus);
 auto stick = [&](int cx, int cy, float sx, float sy, uint32_t bit) {
  bool on = (padHeld & bit) || std::fabs(sx) > .4f || std::fabs(sy) > .4f;
  int radius = S(28);
  if (on) p.circle(cx, cy, radius + 6, ui::alpha(t.acc, 90));
  p.circle(cx, cy, radius, t.btn);
  p.circle(cx + int(sx * S(9)), cy + int(sy * S(9)), radius - S(9), on ? t.acc : ui::mix(t.btn, t.btnText, .25f));
 };
 stick(X(112), Y(118), stickLX, stickLY, pad::LStick);
 stick(X(568), Y(226), stickRX, stickRY, pad::RStick);
 struct Dir { float x, y; Icon icon; uint32_t bit; };
 const Dir dirs[4] = {{112, 190, Icon::Up, pad::Up}, {142, 220, Icon::Right, pad::Right}, {112, 250, Icon::Down, pad::Down}, {82, 220, Icon::Left, pad::Left}};
 for (auto& d : dirs) { padButton(X(d.x), Y(d.y), S(15), "", d.bit); p.icon(d.icon, X(d.x), Y(d.y), S(20), (padHeld & d.bit) ? t.accText : t.btnText); }
 padButton(X(568), Y(92), S(15), "X", pad::X); padButton(X(538), Y(122), S(15), "Y", pad::Y);
 padButton(X(598), Y(122), S(15), "A", pad::A, true); padButton(X(568), Y(152), S(15), "B", pad::B);
 std::string held;
 const std::pair<uint32_t, const char*> names[] = {{pad::A, "A"}, {pad::B, "B"}, {pad::X, "X"}, {pad::Y, "Y"}, {pad::L, "L"}, {pad::R, "R"}, {pad::ZL, "ZL"}, {pad::ZR, "ZR"}, {pad::Plus, "Plus"}, {pad::Minus, "Minus"}, {pad::Up, "Up"}, {pad::Down, "Down"}, {pad::Left, "Left"}, {pad::Right, "Right"}, {pad::LStick, "L stick"}, {pad::RStick, "R stick"}};
 for (auto& n : names) if (padHeld & n.first) { if (!held.empty()) held += " + "; held += n.second; }
 if (!sectionFocus) p.text("Press A to try the buttons.", x + 24, y + h - 70, 16, t.muted, ui::Bold, lw - 48);
 else if (held.empty()) p.text("Press any button. B goes back.", x + 24, y + h - 70, 16, t.muted, ui::Bold, lw - 48);
 else p.text("Holding " + held, x + 24, y + h - 70, 16, t.acc, ui::Bold, lw - 48);
 // lists
 int rx = x + lw + 18, rw = w - lw - 18;
 p.shadow(rx, y, rw, h, rr, 1);
 p.round(rx, y, rw, h, rr, t.surf);
 struct Row { const char* keys; const char* label; };
 static const Row library[] = {{"L R", "Change tab"}, {"A", "Play"}, {"Y", "Favourite"}, {"X", "Search"}, {"ZL ZR", "Change console"}, {"-", "Change sort"}};
 static const Row game[] = {{"+ -", "Together: open the game menu"}, {"ZR ZL", "Quick save / quick load"}, {"R3", "Fast forward"}, {"L3", "Rewind (hold)"}};
 int ly = y + 22;
 auto list = [&](const char* heading, const Row* rows, int n) {
  p.text(heading, rx + 22, ly, 16, t.muted, ui::Bold, rw - 44);
  ly += 32;
  for (int i = 0; i < n; ++i) {
   int kw = std::max(40, p.measure(rows[i].keys, 13, ui::Heavy) + 20);
   p.round(rx + 22, ly, kw, 28, 14, t.btn);
   p.text(rows[i].keys, rx + 22, ly + 7, 13, t.btnText, ui::Heavy, kw, ui::AlignCenter);
   p.text(rows[i].label, rx + 22 + kw + 12, ly + 5, 15, t.text, ui::Bold, rw - kw - 60);
   ly += 36;
  }
  ly += 10;
 };
 list("In the library", library, 6);
 list("While playing", game, 4);
}

// ------------------------------------------------------------------ Map
void LibraryUI::loadMap() {
 mapBlocks.clear();
 mapKnown = 0;
 for (auto& info : platforms()) {
  MapBlock block;
  block.platform = int(info.id);
  std::ifstream in(retroverseAssets() + info.slug + "-database.tsv");
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
   size_t a = line.find('\t'), b = a == std::string::npos ? a : line.find('\t', a + 1), c = b == std::string::npos ? b : line.find('\t', b + 1);
   if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
   std::string title = line.substr(c + 1);
   if (!title.empty() && title.back() == '\r') title.pop_back();
   block.index.emplace(title, n);
   ++n;
  }
  block.known = n;
  mapKnown += n;
  mapBlocks.push_back(std::move(block));
 }
}

void LibraryUI::layoutMap(int left, int width, int top, int maxHeight) {
 mapTop = top;
 const int gap = 6;
 int cell = 4, rows = 0;
 const int gaps = gap * std::max(0, int(mapBlocks.size()) - 1);
 for (; cell >= 2; --cell) {
  int maxRows = maxHeight / cell;
  for (int candidate = 1; candidate <= maxRows; ++candidate) {
   int total = gaps;
   for (auto& b : mapBlocks) total += ((b.known + candidate - 1) / candidate) * cell;
   if (total <= width) { rows = candidate; break; }
  }
  if (rows) break;
 }
 if (!rows) { cell = 2; rows = maxHeight / 2; }
 mapCell = cell;
 mapHeight = rows * cell;
 int total = gaps;
 for (auto& b : mapBlocks) total += ((b.known + rows - 1) / rows) * cell;
 int x = left + std::max(0, (width - total) / 2);
 for (auto& b : mapBlocks) {
  int cols = (b.known + rows - 1) / rows;
  b.x = x; b.w = cols * cell;
  b.dots.clear();
  b.dots.reserve(size_t(b.known));
  for (int j = 0; j < b.known; ++j) { int col = j / rows, row = j % rows; b.dots.push_back({x + col * cell, mapTop + mapHeight - (row + 1) * cell, cell - 1, cell - 1}); }
  x += b.w + gap;
 }
}

void LibraryUI::drawMap(int x, int y, int w, int h) {
 auto& p = paint;
 auto& t = p.theme;
 int rr = p.radius(16);
 p.shadow(x, y, w, h, rr, 1);
 p.round(x, y, w, h, rr, t.surf);
 p.text("Every known release", x + 24, y + 20, 22, t.text, ui::Heavy, 500);
 if (mapState == 0) { p.text("Loading the release lists...", x, y + h / 2, 18, t.muted, ui::Bold, w, ui::AlignCenter); mapState = 1; return; }
 if (mapState == 1) { loadMap(); layoutMap(x + 24, w - 48, y + 96, h - 190); mapState = 2; }
 auto locate = [&](const std::string& game) -> std::pair<int, int> {
  auto pl = detectPlatform(game);
  auto title = gameTitle(game);
  auto find = [&](Platform platform) -> std::pair<int, int> {
   for (size_t i = 0; i < mapBlocks.size(); ++i) {
    auto& b = mapBlocks[i];
    if (b.platform != int(platform)) continue;
    auto hit = b.index.find(title);
    if (hit != b.index.end()) return {int(i), hit->second};
   }
   return {-1, -1};
  };
  auto hit = find(pl);
  if (hit.first < 0 && (pl == Platform::GB || pl == Platform::GBC)) hit = find(pl == Platform::GB ? Platform::GBC : Platform::GB);
  return hit;
 };
 std::string signature = std::to_string(all.size()) + (all.empty() ? std::string() : "|" + all.front() + "|" + all.back());
 if (signature != mapSignature) {
  mapSignature = signature;
  mapOwned.clear();
  for (auto& b : mapBlocks) b.owned = 0;
  std::set<std::pair<int, int>> marked;
  for (auto& game : all) { auto hit = locate(game); if (hit.first >= 0 && marked.insert(hit).second) { mapOwned.push_back(hit); ++mapBlocks[size_t(hit.first)].owned; } }
 }
 std::string own = "You have " + commas((long long)mapOwned.size()) + " of " + commas(mapKnown);
 int ow = p.measure(own, 17, ui::Bold);
 p.text(own, x + w - 24 - ow, y + 24, 17, t.acc, ui::Bold);
 p.text("One dot per release in the lists bundled with the app. Bright dots are games you have.", x + 24, y + 56, 14, t.muted, ui::Medium, w - 48);
 bool any = !mapOwned.empty();
 for (auto& b : mapBlocks) {
  SDL_Color c = ui::consoleColor(b.platform);
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, any ? 70 : 150);
  if (!b.dots.empty()) SDL_RenderFillRects(r, b.dots.data(), int(b.dots.size()));
 }
 for (auto& owned : mapOwned) {
  auto& b = mapBlocks[size_t(owned.first)];
  const SDL_Rect& d = b.dots[size_t(owned.second)];
  SDL_Color c = ui::consoleColor(b.platform);
  p.fill(d.x - 2, d.y - 2, d.w + 4, d.h + 4, c);
 }
 std::string sel = gridIndex < view.size() ? view[gridIndex] : "";
 if (!sel.empty()) {
  auto hit = locate(sel);
  if (hit.first >= 0) {
   const SDL_Rect& d = mapBlocks[size_t(hit.first)].dots[size_t(hit.second)];
   int cx = d.x + d.w / 2, cy = d.y + d.h / 2;
   float pulse = .5f + .5f * std::sin(tick * .12f);
   p.circle(cx, cy, 10 + int(pulse * 4), ui::alpha(t.text, Uint8(40 + pulse * 60)));
   p.circle(cx, cy, 5, t.text);
   std::string label = ui::prettyTitle(gameTitle(sel));
   int lw = std::min(w - 48, p.measure(label, 14, ui::Bold) + 26);
   int lx = std::clamp(cx - lw / 2, x + 24, x + w - 24 - lw), ly = cy - 44 < mapTop ? cy + 14 : cy - 44;
   p.round(lx, ly, lw, 30, 15, t.text);
   p.text(label, lx, ly + 7, 14, t.bg, ui::Bold, lw, ui::AlignCenter);
  }
 }
 int lx = x + 24, ly = mapTop + mapHeight + 14;
 for (auto& b : mapBlocks) {
  const auto& info = platformInfo(Platform(b.platform));
  std::string label = std::string(info.shortName) + "  " + commas(b.owned) + " / " + commas(b.known);
  int tw = p.measure(label, 13, b.owned ? ui::Bold : ui::Medium) + 18;
  if (lx + tw > x + w - 24) { lx = x + 24; ly += 22; }
  p.circle(lx + 5, ly + 8, 5, ui::consoleColor(b.platform));
  p.text(label, lx + 16, ly, 13, b.owned ? t.text : t.muted, b.owned ? ui::Bold : ui::Medium, tw);
  lx += tw + 14;
 }
}

// ------------------------------------------------------------------ About + SD card
static int storageWorker(void* data) {
 auto* ui = static_cast<LibraryUI*>(data);
 const std::string rootDir = ui->storageRoot;
 std::vector<LibraryUI::StorageItem> out;
 auto stopped = [ui] { return ui->storageStop.load(std::memory_order_relaxed); };
 auto walk = [&stopped](const fs::path& dir, const std::function<void(const fs::path&, uintmax_t)>& visit) {
  std::error_code error;
  if (stopped() || !fs::exists(dir, error)) return;
  fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, error), end;
  for (; !error && it != end && !stopped(); it.increment(error)) { std::error_code inner; if (it->is_regular_file(inner)) { auto n = it->file_size(inner); if (!inner) visit(it->path(), n); } }
 };
 std::map<std::string, uintmax_t> roms;
 uintmax_t otherRoms = 0;
 {
  std::error_code error;
  fs::directory_iterator it(rootDir + "roms", fs::directory_options::skip_permission_denied, error), end;
  for (; !error && it != end && !stopped(); it.increment(error)) {
   std::error_code inner;
   if (it->is_directory(inner)) { uintmax_t total = 0; walk(it->path(), [&](const fs::path&, uintmax_t n) { total += n; }); roms[it->path().filename().string()] += total; }
   else if (it->is_regular_file(inner)) { auto n = it->file_size(inner); if (inner) continue; auto ext = lower(it->path().extension().string()); if (ext == ".gba") roms["gba"] += n; else otherRoms += n; }
  }
 }
 for (auto& info : platforms()) { auto found = roms.find(info.slug); if (found != roms.end() && found->second) { out.push_back({std::string("Games: ") + info.shortName, found->second, ui::consoleColor(int(info.id))}); roms.erase(found); } }
 for (auto& rest : roms) otherRoms += rest.second;
 if (otherRoms) out.push_back({"Games: other", otherRoms, ui::rgba(0x8A8F99)});
 uintmax_t saves = 0, states = 0;
 walk(rootDir + "saves", [&](const fs::path& p, uintmax_t n) { auto e = p.extension().string(); if (e == ".state" || e == ".bmp") states += n; else saves += n; });
 if (saves) out.push_back({"Saves", saves, ui::rgba(0x2E9E5B)});
 if (states) out.push_back({"Save states", states, ui::rgba(0x7B5CD6)});
 uintmax_t artwork = 0;
 for (const char* dir : {"covers", "artwork-cache", "artwork"}) walk(rootDir + dir, [&](const fs::path&, uintmax_t n) { artwork += n; });
 if (artwork) out.push_back({"Covers", artwork, ui::rgba(0xE9A23B)});
 const std::pair<const char*, std::pair<const char*, Uint32>> singles[] = {{"screenshots", {"Screenshots", 0xE0663A}}, {"metadata", {"Metadata", 0x2FB3C9}}, {"system", {"BIOS & firmware", 0x4C9A5E}}, {"cheats", {"Cheats", 0xE0517F}}, {"themes", {"Themes", 0x5B6EE1}}};
 for (auto& single : singles) { uintmax_t total = 0; walk(rootDir + single.first, [&](const fs::path&, uintmax_t n) { total += n; }); if (total) out.push_back({single.second.first, total, ui::rgba(single.second.second)}); }
#ifndef RETROVERSE_WINDOWS_PREVIEW
 uintmax_t cores = 0;
 walk("sdmc:/switch/ai1emu/cores", [&](const fs::path&, uintmax_t n) { cores += n; });
 if (cores) out.push_back({"Emulator cores", cores, ui::rgba(0x2F80D1)});
 { std::error_code error; auto n = fs::file_size("sdmc:/switch/ai1emu/AI1Emu.nro", error); if (!error && n) out.push_back({"AI1 Emu app", n, ui::rgba(0xD9463B)}); }
#endif
 std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.bytes > b.bytes; });
 { std::lock_guard<std::mutex> lock(ui->storageMutex); ui->storage = std::move(out); }
 ui->storageState = 2;
 return 0;
}

void LibraryUI::stopStorageScan() {
 if (!storageThread) return;
 bool incomplete = storageState.load() != 2;
 storageStop = true;
 SDL_WaitThread(storageThread, nullptr);
 storageThread = nullptr;
 if (incomplete) { std::lock_guard<std::mutex> lock(storageMutex); storage.clear(); storageState = 0; }
 storageStop = false;
}

struct TreeBox { double x, y, w, h; int index; };
static std::vector<TreeBox> squarify(const std::vector<double>& values, double X, double Y, double W, double H) {
 std::vector<TreeBox> out;
 double total = 0;
 for (double v : values) total += v;
 if (total <= 0 || W <= 0 || H <= 0) return out;
 double scale = W * H / total, x = X, y = Y, w = W, h = H;
 std::vector<std::pair<double, int>> rest, row;
 for (size_t i = 0; i < values.size(); ++i) if (values[i] > 0) rest.push_back({values[i] * scale, int(i)});
 auto worst = [](const std::vector<std::pair<double, int>>& items, double side) {
  double sum = 0, most = 0, least = 1e300;
  for (auto& q : items) { sum += q.first; most = std::max(most, q.first); least = std::min(least, q.first); }
  if (sum <= 0 || side <= 0) return 1e300;
  return std::max(side * side * most / (sum * sum), (sum * sum) / (side * side * least));
 };
 auto flush = [&] {
  double sum = 0;
  for (auto& q : row) sum += q.first;
  if (w >= h) { double rw = sum / h, cy = y; for (auto& q : row) { double rh = q.first / rw; out.push_back({x, cy, rw, rh, q.second}); cy += rh; } x += rw; w -= rw; }
  else { double rh = sum / w, cx = x; for (auto& q : row) { double rw = q.first / rh; out.push_back({cx, y, rw, rh, q.second}); cx += rw; } y += rh; h -= rh; }
  row.clear();
 };
 size_t k = 0;
 while (k < rest.size()) { double side = std::min(w, h); auto candidate = row; candidate.push_back(rest[k]); if (row.empty() || worst(candidate, side) <= worst(row, side)) { row = candidate; ++k; } else flush(); }
 if (!row.empty()) flush();
 return out;
}

void LibraryUI::drawAbout(int x, int y, int w, int h) {
 auto& p = paint;
 auto& t = p.theme;
 int rr = p.radius(16), lw = 360;
 p.shadow(x, y, lw, h, rr, 1);
 p.round(x, y, lw, h, rr, t.surf);
 p.round(x + 24, y + 24, 56, 56, p.radius(14), t.acc);
 p.text("AI1", x + 24, y + 41, 19, t.accText, ui::Heavy, 56, ui::AlignCenter);
 p.text("AI1 Emu", x + 96, y + 26, 24, t.text, ui::Heavy, lw - 110);
 p.text(std::string("Version ") + AI1EMU_VERSION, x + 96, y + 58, 15, t.muted, ui::Medium, lw - 110);
 const char* cores[] = {"mGBA", "FCEUmm", "Snes9x", "Genesis Plus GX", "melonDS", "Mupen64Plus-Next", "Dekopon (3DS)"};
 int cx = x + 24, cy = y + 106;
 for (auto* core : cores) {
  int cw = p.measure(core, 13, ui::Bold) + 24;
  if (cx + cw > x + lw - 24) { cx = x + 24; cy += 34; }
  p.round(cx, cy, cw, 26, 13, t.surf2);
  p.text(core, cx, cy + 6, 13, t.text, ui::Bold, cw, ui::AlignCenter);
  cx += cw + 8;
 }
 p.paragraph("Plays offline at normal clock speeds. Battery saves happen automatically, and your place is saved when you quit from the game menu.", x + 24, cy + 52, 15, t.muted, lw - 48, 23, 5);
 p.paragraph("Fonts: M PLUS Rounded 1c, Rubik, Nunito and Geist Mono (SIL OFL). stb libraries: MIT or public domain.", x + 24, y + h - 70, 12, t.muted, lw - 48, 18, 3);
 int sx = x + lw + 18, sw = w - lw - 18;
 p.shadow(sx, y, sw, h, rr, 1);
 p.round(sx, y, sw, h, rr, t.surf);
 p.text("Your SD card", sx + 24, y + 20, 22, t.text, ui::Heavy, 300);
 if (storageState == 0 && !storageThread) { storageState = 1; storageThread = SDL_CreateThread(storageWorker, "ai1-storage", this); if (!storageThread) storageState = 2; }
 if (storageState == 2 && storageThread) { SDL_WaitThread(storageThread, nullptr); storageThread = nullptr; }
 std::vector<StorageItem> items;
 { std::lock_guard<std::mutex> lock(storageMutex); items = storage; }
 const double TX = sx + 24, TY = y + 70, TW = sw - 48, TH = h - 120;
 if (storageState != 2) { std::string dots(size_t(1 + (tick / 20) % 3), '.'); p.text("Measuring your library" + dots, sx, y + h / 2, 18, t.muted, ui::Bold, sw, ui::AlignCenter); }
 else if (items.empty()) p.text("Nothing to measure yet.", sx, y + h / 2, 17, t.muted, ui::Medium, sw, ui::AlignCenter);
 else {
  uintmax_t total = 0;
  std::vector<double> values;
  for (auto& item : items) { total += item.bytes; values.push_back(double(item.bytes)); }
  std::string sum = bytesLabel(total) + " in total";
  int tw = p.measure(sum, 16, ui::Bold);
  p.text(sum, sx + sw - 24 - tw, y + 26, 16, t.muted, ui::Bold);
  for (auto& box : squarify(values, TX, TY, TW, TH)) {
   auto& item = items[size_t(box.index)];
   int bx = int(box.x + .5), by = int(box.y + .5), bw = int(box.x + box.w + .5) - bx, bh = int(box.y + box.h + .5) - by;
   if (bw < 8 || bh < 8) continue;
   p.round(bx + 2, by + 2, bw - 4, bh - 4, std::min(p.radius(10), std::min(bw, bh) / 3), item.color);
   SDL_Color ink = ui::onColor(item.color);
   if (bw >= 104 && bh >= 48) { p.text(item.name, bx + 12, by + 10, 14, ink, ui::Bold, bw - 22); p.text(bytesLabel(item.bytes), bx + 12, by + 30, 13, ink, ui::Medium, bw - 22); }
   else if (bw >= 56 && bh >= 26) p.text(bytesLabel(item.bytes), bx + 8, by + 6, 12, ink, ui::Medium, bw - 12);
  }
 }
 p.text("Measured once per session in " + storageRoot, sx + 24, y + h - 34, 12, t.muted, ui::Medium, sw - 48);
}
