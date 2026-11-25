// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// v0.5 persistent data shared by the library and every emulator runner:
// settings, themes, favourites, play history, cheats and save-state slots.
// Everything is a small text file under the data root, so it survives updates.
#pragma once
#include "ui_kit.h"
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

// ------------------------------------------------------------------ settings
// Gameplay options can be set for all games or overridden for one game.
struct GameplaySettings {
 int ffSpeed = 1;      // index into fastForwardSpeeds(): 1.5x, 2x, 3x, 4x
 bool ffToggle = false; // false: hold the button, true: press to toggle
 bool rewind = true;
 int screen = 0;       // 0 fit, 1 pixel perfect, 2 stretch
 int look = 0;         // 0 sharp, 1 smooth, 2 LCD grid, 3 CRT lines
 bool muteFastForward = true;
 int volume = 3;       // 0 mute, 1 50%, 2 75%, 3 100%
 bool boost = false;   // raise the CPU clock while playing (more heat and battery use)
 bool dsThreaded = true; // DS: draw 3D on a second CPU core (applies when the game starts)
};
const float* fastForwardSpeeds(); // 4 entries
const char* fastForwardLabel(int index);
float volumeScale(int volume);

struct AppSettings {
 std::string theme = "white";  // preset id or "custom"
 std::string accent;           // optional "#RRGGBB" override
 int font = 0;                 // ui::FontFamily
 int corners = 1;              // 0 square, 1 soft, 2 round
 bool wallpaper = false;       // use themes/wallpaper.png|jpg as the background
 int sort = 0;                 // 0 recently played, 1 A-Z, 2 Z-A, 3 most played
 bool tipShown = false;        // first-game "open the menu" tip
 GameplaySettings gameplay;
};
std::string settingsPath(const std::string& root);
AppSettings loadSettings(const std::string& root);
bool saveSettings(const std::string& root, const AppSettings& s);
// Per-game gameplay overrides live next to the save files: <prefix>.options
bool hasGameOverrides(const std::string& prefix);
GameplaySettings loadGameplay(const std::string& root, const std::string& prefix);
bool saveGameOverrides(const std::string& prefix, const GameplaySettings& g);
void clearGameOverrides(const std::string& prefix);

// Resolved theme for the current settings (preset + custom accent).
ui::Theme resolveTheme(const AppSettings& s);
// "Save my theme": writes themes/my-theme.theme and makes it selectable.
bool saveCustomTheme(const std::string& root, const AppSettings& s);
bool loadCustomTheme(const std::string& root, AppSettings& s);
std::string wallpaperPath(const std::string& root); // without extension

// ------------------------------------------------------------------ library data
struct PlayRecord { long long lastPlayed = 0; long long seconds = 0; };
std::map<std::string, PlayRecord> loadHistory(const std::string& root);
// Adds a finished play session (called by the runners when a game closes).
void recordSession(const std::string& root, const std::string& game, long long started, long long seconds);
std::set<std::string> loadFavorites(const std::string& root);
bool saveFavorites(const std::string& root, const std::set<std::string>& favorites);

std::string formatPlaytime(long long seconds, bool shortForm = false);
std::string formatWhen(long long stamp, std::time_t now);

// ------------------------------------------------------------------ cheats
struct Cheat { std::string description, code; bool enabled = false; };
std::string cheatPath(const std::string& root, const std::string& game);
std::vector<Cheat> loadCheats(const std::string& file);
bool saveCheats(const std::string& file, const std::vector<Cheat>& cheats);
std::vector<std::string> cheatFilesFor(const std::string& root, const std::string& game);

// ------------------------------------------------------------------ save slots
// Slot 0 is the automatic save made on quit; slot 1 is the existing quick state
// (".state", so v0.4 states keep working); slots 2-5 are extra manual slots.
constexpr int slotCount = 6;
std::string slotStatePath(const std::string& prefix, int slot);
std::string slotThumbPath(const std::string& prefix, int slot);
long long slotTime(const std::string& prefix, int slot); // 0 when empty
int usedSlots(const std::string& prefix);
