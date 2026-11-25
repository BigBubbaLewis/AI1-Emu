// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
#include "app_data.h"
#include "safe_file.h"
#include "platform_library.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;

namespace {
std::map<std::string, std::string> readKeyValues(const std::string& path) {
 std::map<std::string, std::string> out;
 std::ifstream in(path);
 if(!in) in.open(path+".previous");
 std::string line;
 while (std::getline(in, line)) {
  if (!line.empty() && line.back() == '\r') line.pop_back();
  auto eq = line.find('=');
  if (eq == std::string::npos || line[0] == '#') continue;
  out[line.substr(0, eq)] = line.substr(eq + 1);
 }
 return out;
}
bool writeAtomic(const std::string& path, const std::string& content) {
 return safeWriteFile(path, content.data(), content.size());
}
int toInt(const std::map<std::string, std::string>& kv, const char* key, int fallback, int lo, int hi) {
 auto it = kv.find(key);
 if (it == kv.end()) return fallback;
 try { return std::clamp(std::stoi(it->second), lo, hi); } catch (...) { return fallback; }
}
void readGameplay(const std::map<std::string, std::string>& kv, GameplaySettings& g) {
 g.ffSpeed = toInt(kv, "ff_speed", g.ffSpeed, 0, 3);
 g.ffToggle = toInt(kv, "ff_toggle", g.ffToggle, 0, 1) != 0;
 g.rewind = toInt(kv, "rewind", g.rewind, 0, 1) != 0;
 g.screen = toInt(kv, "screen", g.screen, 0, 2);
 g.look = toInt(kv, "look", g.look, 0, 3);
 g.muteFastForward = toInt(kv, "mute_ff", g.muteFastForward, 0, 1) != 0;
 g.volume = toInt(kv, "volume", g.volume, 0, 3);
 g.boost = toInt(kv, "boost", g.boost, 0, 1) != 0;
 g.dsThreaded = toInt(kv, "ds_threaded", g.dsThreaded, 0, 1) != 0;
}
std::string writeGameplay(const GameplaySettings& g) {
 std::ostringstream o;
 o << "ff_speed=" << g.ffSpeed << "\nff_toggle=" << g.ffToggle << "\nrewind=" << g.rewind << "\nscreen=" << g.screen
   << "\nlook=" << g.look << "\nmute_ff=" << g.muteFastForward << "\nvolume=" << g.volume << "\nboost=" << g.boost
   << "\nds_threaded=" << g.dsThreaded << "\n";
 return o.str();
}
} // namespace

const float* fastForwardSpeeds() { static const float s[4] = {1.5f, 2.f, 3.f, 4.f}; return s; }
const char* fastForwardLabel(int i) { static const char* l[4] = {"1.5\xC3\x97", "2\xC3\x97", "3\xC3\x97", "4\xC3\x97"}; return l[std::clamp(i, 0, 3)]; }
float volumeScale(int v) { static const float s[4] = {0.f, .5f, .75f, 1.f}; return s[std::clamp(v, 0, 3)]; }

std::string settingsPath(const std::string& root) { return root + "settings.txt"; }

AppSettings loadSettings(const std::string& root) {
 AppSettings s;
 auto kv = readKeyValues(settingsPath(root));
 if (kv.count("theme")) s.theme = kv["theme"];
 if (kv.count("accent")) s.accent = kv["accent"];
 s.font = toInt(kv, "font", s.font, 0, ui::FontFamilyCount - 1);
 s.corners = toInt(kv, "corners", s.corners, 0, 2);
 s.wallpaper = toInt(kv, "wallpaper", s.wallpaper, 0, 1) != 0;
 s.sort = toInt(kv, "sort", s.sort, 0, 3);
 s.tipShown = toInt(kv, "tip_shown", s.tipShown, 0, 1) != 0;
 readGameplay(kv, s.gameplay);
 return s;
}

bool saveSettings(const std::string& root, const AppSettings& s) {
 std::ostringstream o;
 o << "# AI1 Emu settings\ntheme=" << s.theme << "\naccent=" << s.accent << "\nfont=" << s.font << "\ncorners=" << s.corners
   << "\nwallpaper=" << s.wallpaper << "\nsort=" << s.sort << "\ntip_shown=" << s.tipShown << "\n" << writeGameplay(s.gameplay);
 return writeAtomic(settingsPath(root), o.str());
}

bool hasGameOverrides(const std::string& prefix) { std::error_code e; return fs::exists(prefix + ".options", e); }
GameplaySettings loadGameplay(const std::string& root, const std::string& prefix) {
 GameplaySettings g = loadSettings(root).gameplay;
 if (hasGameOverrides(prefix)) readGameplay(readKeyValues(prefix + ".options"), g);
 return g;
}
bool saveGameOverrides(const std::string& prefix, const GameplaySettings& g) { return writeAtomic(prefix + ".options", writeGameplay(g)); }
void clearGameOverrides(const std::string& prefix) { std::remove((prefix + ".options").c_str()); }

ui::Theme resolveTheme(const AppSettings& s) {
 ui::Theme t = ui::presetTheme(s.theme == "custom" ? "white" : s.theme);
 if (s.theme == "custom") { t.id = "custom"; t.name = "My theme"; }
 SDL_Color accent;
 if (!s.accent.empty() && ui::parseHex(s.accent, accent)) { t.acc = accent; t.accText = ui::onColor(accent); }
 return t;
}

bool saveCustomTheme(const std::string& root, const AppSettings& s) {
 ui::Theme base = ui::presetTheme(s.theme == "custom" ? "white" : s.theme);
 std::ostringstream o;
 o << "base=" << base.id << "\naccent=" << s.accent << "\nfont=" << s.font << "\ncorners=" << s.corners << "\nwallpaper=" << s.wallpaper << "\n";
 return writeAtomic(root + "themes/my-theme.theme", o.str());
}

bool loadCustomTheme(const std::string& root, AppSettings& s) {
 auto kv = readKeyValues(root + "themes/my-theme.theme");
 if (kv.empty()) return false;
 s.theme = kv.count("base") ? kv["base"] : "white";
 s.accent = kv.count("accent") ? kv["accent"] : "";
 s.font = toInt(kv, "font", 0, 0, ui::FontFamilyCount - 1);
 s.corners = toInt(kv, "corners", 1, 0, 2);
 s.wallpaper = toInt(kv, "wallpaper", 0, 0, 1) != 0;
 return true;
}

std::string wallpaperPath(const std::string& root) { return root + "themes/wallpaper"; }

// ------------------------------------------------------------------ history
// history.txt: one line per game, "<last played>\t<seconds>\t<path>".
std::map<std::string, PlayRecord> loadHistory(const std::string& root) {
 std::map<std::string, PlayRecord> out;
 std::ifstream in(root + "history.txt");
 std::string line;
 while (std::getline(in, line)) {
  if (!line.empty() && line.back() == '\r') line.pop_back();
  auto a = line.find('\t'), b = a == std::string::npos ? a : line.find('\t', a + 1);
  if (b == std::string::npos) continue;
  try { out[line.substr(b + 1)] = {std::stoll(line.substr(0, a)), std::stoll(line.substr(a + 1, b - a - 1))}; } catch (...) {}
 }
 return out;
}

void recordSession(const std::string& root, const std::string& game, long long started, long long seconds) {
 auto history = loadHistory(root);
 auto& r = history[game];
 r.lastPlayed = std::max(r.lastPlayed, started);
 r.seconds += std::max(0LL, seconds);
 std::ostringstream o;
 for (auto& h : history) o << h.second.lastPlayed << '\t' << h.second.seconds << '\t' << h.first << '\n';
 writeAtomic(root + "history.txt", o.str());
}

std::set<std::string> loadFavorites(const std::string& root) {
 std::set<std::string> out;
 std::ifstream in(root + "favorites.txt");
 std::string line;
 while (std::getline(in, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); if (!line.empty()) out.insert(line); }
 return out;
}
bool saveFavorites(const std::string& root, const std::set<std::string>& favorites) {
 std::string content;
 for (auto& f : favorites) content += f + "\n";
 return writeAtomic(root + "favorites.txt", content);
}

std::string formatPlaytime(long long s, bool shortForm) {
 if (s < 60) return s <= 0 ? "Not played yet" : "Under a minute";
 long long h = s / 3600, m = (s % 3600) / 60;
 char b[48];
 if (shortForm) { if (h) std::snprintf(b, sizeof b, "%lld hr%s", h, h == 1 ? "" : "s"); else std::snprintf(b, sizeof b, "%lld min", m); }
 else if (h && m) std::snprintf(b, sizeof b, "%lld hr%s %lld min", h, h == 1 ? "" : "s", m);
 else if (h) std::snprintf(b, sizeof b, "%lld hr%s", h, h == 1 ? "" : "s");
 else std::snprintf(b, sizeof b, "%lld min", m);
 return b;
}

std::string formatWhen(long long stamp, std::time_t now) {
 if (stamp <= 0) return "Never";
 std::time_t t = std::time_t(stamp);
 std::tm then = *std::localtime(&t), today = *std::localtime(&now);
 then.tm_hour = then.tm_min = then.tm_sec = 0; today.tm_hour = today.tm_min = today.tm_sec = 0;
 long long days = (long long)std::difftime(std::mktime(&today), std::mktime(&then)) / 86400;
 if (days <= 0) return "Today";
 if (days == 1) return "Yesterday";
 if (days < 7) return std::to_string(days) + " days ago";
 if (days < 14) return "Last week";
 char b[32];
 std::strftime(b, sizeof b, "%b %d", std::localtime(&t));
 return b;
}

// ------------------------------------------------------------------ cheats (RetroArch .cht format)
std::string cheatPath(const std::string& root, const std::string& game) {
 return root + "cheats/" + platformInfo(detectPlatform(game)).slug + "/" + fs::path(game).stem().string() + ".cht";
}

std::vector<Cheat> loadCheats(const std::string& file) {
 auto kv = readKeyValues(file);
 auto unquote = [](std::string v) {
  while (!v.empty() && v.front() == ' ') v.erase(0, 1);
  if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
  return v;
 };
 std::vector<Cheat> out;
 int n = 0;
 try { n = std::clamp(std::stoi(unquote(kv["cheats "])), 0, 512); } catch (...) {}
 if (!n) try { n = std::clamp(std::stoi(unquote(kv["cheats"])), 0, 512); } catch (...) {}
 for (int i = 0; i < n; ++i) {
  std::string p = "cheat" + std::to_string(i) + "_";
  auto get = [&](const std::string& k) { auto it = kv.find(p + k + " "); if (it == kv.end()) it = kv.find(p + k); return it == kv.end() ? std::string() : unquote(it->second); };
  Cheat c;
  c.description = get("desc");
  c.code = get("code");
  c.enabled = get("enable") == "true";
  if (c.description.empty()) c.description = "Cheat " + std::to_string(i + 1);
  if (!c.code.empty()) out.push_back(c);
 }
 return out;
}

bool saveCheats(const std::string& file, const std::vector<Cheat>& cheats) {
 std::ostringstream o;
 auto safe=[](std::string v){for(char& c:v)if(c=='"'||static_cast<unsigned char>(c)<32)c=' ';return v;};
 o << "cheats = " << cheats.size() << "\n\n";
 for (size_t i = 0; i < cheats.size(); ++i)
  o << "cheat" << i << "_desc = \"" << safe(cheats[i].description) << "\"\ncheat" << i << "_code = \"" << safe(cheats[i].code) << "\"\ncheat" << i << "_enable = " << (cheats[i].enabled ? "true" : "false") << "\n\n";
 return writeAtomic(file, o.str());
}

std::vector<std::string> cheatFilesFor(const std::string& root, const std::string& game) {
 std::vector<std::string> out;
 std::error_code e;
 std::string dir = root + "cheats/" + platformInfo(detectPlatform(game)).slug;
 for (fs::directory_iterator it(dir, e), end; !e && it != end; it.increment(e))
  if (it->path().extension() == ".cht" && it->path().string() != cheatPath(root, game)) out.push_back(it->path().string());
 std::sort(out.begin(), out.end());
 return out;
}

// ------------------------------------------------------------------ slots
std::string slotStatePath(const std::string& prefix, int slot) {
 if (slot == 0) return prefix + ".auto.state";
 if (slot == 1) return prefix + ".state";
 return prefix + ".slot" + std::to_string(slot) + ".state";
}
std::string slotThumbPath(const std::string& prefix, int slot) { return slotStatePath(prefix, slot) + ".bmp"; }
long long slotTime(const std::string& prefix, int slot) {
 std::error_code e;
 auto t = fs::last_write_time(slotStatePath(prefix, slot), e);
 if (e) return 0;
 auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(t - decltype(t)::clock::now() + std::chrono::system_clock::now());
 return (long long)std::chrono::system_clock::to_time_t(sys);
}
int usedSlots(const std::string& prefix) {
 int n = 0;
 std::error_code e;
 for (int i = 0; i < slotCount; ++i) if (fs::exists(slotStatePath(prefix, i), e)) ++n;
 return n;
}
