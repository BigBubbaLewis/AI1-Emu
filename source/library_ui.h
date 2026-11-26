// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// v0.5 library: Home, Library and Settings, drawn with the shared ui kit.
// The frontend (Switch main.cpp or the desktop preview) feeds it controller bits
// and touch events and acts on the Command it returns.
#pragma once
#include "app_data.h"
#include "ui_kit.h"
#include <SDL.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct LibraryUI {
 enum TabId { TabHome = 0, TabLibrary = 1, TabSettings = 2 };
 static constexpr int tabCount = 3;
 enum Section { SecLibrary, SecGameplay, SecControls, SecThemes, SecUpdates, SecMap, SecAbout, SectionCount };
 struct Command {
  enum Type { None, Launch, Quit, Search, Update } type = None;
  std::string path;
  bool resume = false;
 };

 explicit LibraryUI(SDL_Renderer* renderer);
 ~LibraryUI();
 LibraryUI(const LibraryUI&) = delete;
 LibraryUI& operator=(const LibraryUI&) = delete;

 void setGames(const std::vector<std::string>& games);
 void setQuery(const std::string& q);
 // Controller input for one frame (pad:: bits). Handles key repeat itself.
 Command update(uint32_t down, uint32_t held);
 Command handlePointer(const SDL_Event& e);
 void draw(bool present = true);
 void toast(const std::string& message);
 void showLaunching(const std::string& game); // "Starting..." card, presented at once
 void stopStorageScan();
 std::string coverFor(const std::string& game);
 std::string selectedGame() const;
 void select(const std::string& game);
 void reloadData(); // history, favourites and settings (after returning from a game)

 SDL_Renderer* r;
 ui::Painter paint;
 std::string root;
 AppSettings settings;
 std::map<std::string, PlayRecord> history;
 std::set<std::string> favorites;
 std::vector<std::string> all, sorted, view, recent;
 std::vector<int> consoles; // platforms that have games, in console order
 int filter = 0;            // 0 all, 1 favourites, 2+ consoles[filter-2]
 std::string query;

 int tab = TabHome;
 int homeZone = 0, homeIndex = 0; // 0 hero buttons, 1 recent list, 2 shelf
 size_t shelfIndex = 0, gridIndex = 0;
 float shelfScroll = 0, gridScroll = 0;
 // Scrolling follows the selection after D-pad moves; after a touch drag it stays
 // where the finger left it and glides with momentum instead of snapping back.
 bool gridFollow = true, shelfFollow = true;
 float gridVelocity = 0, shelfVelocity = 0;
 float frameDt = 1.f / 60;
 int section = SecLibrary, settingRow = 0;
 bool sectionFocus = false;
 int themeCard = 0;
 std::vector<SDL_Rect> chipRects; // Library console filters as last drawn (empty rect = not visible)

 // Frontend-provided state
 int battery = -1; bool charging = false;
 uint32_t padHeld = 0; float stickLX = 0, stickLY = 0, stickRX = 0, stickRY = 0;
 std::string updateButton = "Check for updates", updateStatus;
 bool updateBusy = false;
 bool updateRequested = false;

 // Art cache. Covers are decoded on a background thread so the screen never waits
 // for the SD card; state 0 = loading, 1 = ready, 2 = no artwork.
 struct Art { SDL_Texture* texture = nullptr; int w = 0, h = 0; unsigned used = 0; int state = 1; };
 std::map<std::string, Art> images;
 unsigned tick = 0;
 Art& art(const std::string& pathWithoutExtension);
 void pumpArt(int maxUploads = 3);  // upload decoded covers (main thread)
 bool artIdle();                     // true when no cover is waiting to load (tests)
 // Missing covers, found a few games per call so the library stays smooth.
 // Each result is {download URL, cover path without extension}.
 std::vector<std::pair<std::string, std::string>> missingCovers(int budget);
 void coverArrived(const std::string& pathWithoutExtension); // reload a downloaded cover
 size_t coverScan = 0;

 // Map (Settings > Game map)
 struct MapBlock { int platform = 0; int known = 0; int owned = 0; int x = 0, w = 0; std::unordered_map<std::string, int> index; std::vector<SDL_Rect> dots; };
 std::vector<MapBlock> mapBlocks;
 int mapState = 0, mapKnown = 0, mapCell = 3, mapTop = 0, mapHeight = 0;
 std::string mapSignature;
 std::vector<std::pair<int, int>> mapOwned;

 // About: SD card breakdown, measured once per session on a background thread.
 struct StorageItem { std::string name; uintmax_t bytes = 0; SDL_Color color{255, 255, 255, 255}; };
 std::vector<StorageItem> storage;
 std::mutex storageMutex;
 std::atomic<int> storageState{0};
 std::atomic<bool> storageStop{false};
 SDL_Thread* storageThread = nullptr;
 std::string storageRoot;

private:
 struct Hit { SDL_Rect rect; std::function<void()> action; };
 std::vector<Hit> hits;
 struct GameInfo { int states = 0, cheatsOn = 0; bool autoSave = false; Uint64 at = 0; };
 std::map<std::string, GameInfo> infoCache;
 const GameInfo& infoFor(const std::string& game);
 Art loadImage(const std::string& pathWithoutExtension, int maxDim);
 bool quitArmed = false;
 Command pending;
 std::string toastText;
 Uint64 toastUntil = 0;
 uint32_t repeatBits = 0;
 Uint64 repeatAt = 0;
 bool pointerDown = false, dragging = false;
 int dragZone = 0; // 1 grid, 2 shelf
 float pressX = 0, pressY = 0, dragStart = 0;
 float dragVelocity = 0, lastDragPos = 0;
 Uint64 lastDragTime = 0, frameTicks = 0;
 // Background cover decoder
 struct Decoded { std::string path; std::vector<unsigned char> rgba; int w = 0, h = 0; bool ok = false; };
 std::mutex artMutex;
 std::condition_variable artWake;
 std::deque<std::string> artRequests;
 std::deque<Decoded> artDone;
 std::atomic<bool> artStop{false};
 SDL_Thread* artThread = nullptr;
 static int artWorker(void* self);
 void prewarmFonts();
 SDL_FingerID finger = 0;
 Art wallpaper;
 bool wallpaperTried = false;

 void rebuild();
 void applyTheme();
 void persistSettings();
 void toggleFavorite(const std::string& game);
 void launch(const std::string& game, bool resume);
 bool hasAutoSave(const std::string& game);
 int settingRows() const;
 void adjustSetting(int direction);
 void activateSetting();
 void moveHome(uint32_t down);
 void moveGrid(uint32_t down);
 void moveSettings(uint32_t down);
 void onHit(int x, int y, int w, int h, std::function<void()> action);

 void drawBackground();
 void drawTop();
 void drawFooter(const std::vector<std::pair<std::string, std::string>>& hints, const std::vector<uint32_t>& bits);
 void drawHome();
 void drawLibrary();
 void drawSettings();
 void drawSection(int x, int y, int w, int h);
 void drawControls(int x, int y, int w, int h);
 void drawThemes(int x, int y, int w, int h);
 void drawMap(int x, int y, int w, int h);
 void drawAbout(int x, int y, int w, int h);
 void drawCover(const std::string& game, int x, int y, int w, int h, int radius, SDL_Color under, bool showStar);
 void drawToast();
 void segment(int x, int y, int rightEdge, const std::vector<std::string>& labels, int selected, bool focused, const std::function<void(int)>& onTap);
 void toggle(int x, int y, bool on);
 void padButton(int cx, int cy, int radius, const char* label, uint32_t bit, bool accent = false);
 void padPill(int x, int y, int w, int h, const char* label, uint32_t bit);
 void loadMap();
 void layoutMap(int left, int width, int top, int maxHeight);
};
