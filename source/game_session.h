// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Everything that happens around a running game, shared by all emulator runners:
// the in-game menu, save-state slots with pictures, rewind, fast forward, cheats,
// display options, volume, on-screen messages and play-time tracking.
// It only uses SDL and the libretro entry points passed in, so the desktop
// preview can drive it too.
#pragma once
#include "app_data.h"
#include "online_cheats.h"
#include "platform_library.h"
#include "ui_kit.h"
#include <SDL.h>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct CoreHooks {
 std::function<size_t()> serializeSize;
 std::function<bool(void*, size_t)> serialize;
 std::function<bool(const void*, size_t)> unserialize;
 std::function<void()> reset;
 std::function<void()> cheatReset;
 std::function<void(unsigned, bool, const char*)> cheatSet;
 // Optional: provide the current picture when the runner cannot use captureFrame (N64).
 std::function<bool(std::vector<Uint32>& argb, int& w, int& h)> grabFrame;
};

struct SessionOptions {
 bool states = true, rewind = true, cheats = true, screenModes = true, looks = true;
 bool dualScreen = false, n64 = false;
 bool hotkeys = true; // stick-click fast forward / rewind (off for DS: sticks drive the touch cursor)
 std::string statesNote, cheatsNote;
 int* dsLayout = nullptr; // DS screen layout, changed from the Display page
 std::function<void(bool)> setBoost; // runner-provided CPU clock switch (Switch only)
};

class GameSession {
public:
 GameSession(SDL_Renderer* renderer, std::string root, std::string game, Platform platform, std::string prefix,
             std::string coverPath, CoreHooks hooks, SessionOptions options);
 ~GameSession();

 // Call once per loop with Switch buttons mapped to pad:: bits. Returns false when the player chose to quit.
 bool update(uint32_t held, uint32_t down, bool cornerTap = false);
 bool menuOpen() const { return open; }
 // Number of emulated frames to run this loop (0 while paused). Present only the last one.
 int framesThisLoop();
 bool rewinding() const { return rewindActive; }
 bool fastForward() const { return ffActive; }
 void afterFrame(); // once per emulated frame: rewind snapshots and timing

 // Video helpers for runners that draw with SDL.
 void captureFrame(const void* data, unsigned w, unsigned h, size_t pitch, unsigned pixelFormat);
 SDL_Rect gameRect(int w, int h, double aspect) const;
 SDL_ScaleMode scaleMode() const;
 void drawLook(const SDL_Rect& dst, int sourceHeight);
 void drawOverlay(); // messages over the running game
 // Draws the whole paused screen: the game picture (via the callback), the menu and dialogs.
 void drawMenu(const std::function<void()>& drawGame);

 float audioScale() const; // 0 mutes
 // Saves the automatic slot and play time. Safe to call more than once.
 void finish(bool autoSave);
 bool loadSlot(int slot, bool silent = false);
 bool saveSlot(int slot, bool silent = false);
 void toast(const std::string& message, ui::Icon icon = ui::Icon::Check);
 const GameplaySettings& gameplay() const { return settings; }
 ui::Painter& painter() { return *paint; }

private:
 SDL_Renderer* r;
 std::unique_ptr<ui::Painter> paint;
 std::string root, game, prefix, title, coverFile;
 Platform platform;
 CoreHooks hooks;
 SessionOptions opt;
 AppSettings app;
 GameplaySettings settings;
 bool perGame = false;
 std::vector<Cheat> cheats;
 std::string cheatFile;

 bool open = false, quitRequested = false, finished = false;
 int item = 0, row = 0;          // main list item and row inside the right pane
 bool paneFocus = false;
 int confirm = 0;                // pending dialog (see .cpp)
 int confirmSlot = 0;
 bool browsingFiles = false, browsingOnline = false;
 OnlineCheats onlineCheats;
 OnlineCheats::Phase cheatPhase = OnlineCheats::Idle;
 std::vector<CheatCandidate> onlineChoices;
 void pollCheats();
 std::vector<std::string> files;
 bool settingsDirty = false;

 // play time
 long long started = 0;
 Uint64 playMs = 0, lastTick = 0;

 // fast forward / rewind
 bool ffActive = false, ffHeldPrev = false;
 float ffAccumulator = 0;
 bool rewindActive = false;
 std::deque<std::vector<unsigned char>> rewindBuffer;
 size_t rewindBytes = 0, rewindLimit = 48u * 1024 * 1024;
 unsigned frameCounter = 0, rewindInterval = 4;
 std::vector<unsigned char> rewindScratch;
 std::vector<char> undoState;

 // last frame for thumbnails
 std::vector<unsigned char> frameData;
 unsigned frameW = 0, frameH = 0, framePixel = 0;
 size_t framePitch = 0;

 // messages
 std::string toastText;
 ui::Icon toastIcon = ui::Icon::Check;
 Uint64 toastUntil = 0, tipUntil = 0;

 // cached textures
 struct Pic { SDL_Texture* t = nullptr; int w = 0, h = 0; long long stamp = -1; };
 Pic cover, slotPics[slotCount];

 void openMenu();
 void closeMenu();
 void handleMenu(uint32_t down);
 void activate();
 void adjust(int direction);
 int rowCount() const;
 void saveSettings();
 void applyCheats();
 bool grab(std::vector<Uint32>& argb, int& w, int& h);
 bool writeThumb(const std::string& path, int maxW, int maxH);
 bool takeScreenshot();
 void captureRewind();
 bool stepRewind();
 Pic& slotPicture(int slot);
 void loadCover();

 void drawList(int x, int y, int w, int h);
 void drawPane(int x, int y, int w, int h);
 void drawSlots(int x, int y, int w, bool loading);
 void drawRows(int x, int y, int w);
 void drawDialog();
 void drawFooter(int x, int y, int w, const std::vector<std::pair<std::string, std::string>>& hints);
 void segment(int x, int y, int w, const std::vector<std::string>& labels, int selected, bool focused);
 void toggle(int x, int y, bool on);
};
