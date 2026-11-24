#pragma once
#include <SDL.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <ctime>
#include <cstdint>
struct LibraryUI {
 SDL_Renderer* r;
 // v0.4 typography: Unbounded (display), Geist (body/bold), Geist Mono (labels).
 // Every family falls back to DejaVu Sans when its file is missing.
 enum Family { Display=0, Body=1, BodyBold=2, Mono=3 };
 struct Font { SDL_Texture* atlas=nullptr; void* chars=nullptr; float height=0; int atlasSize=512; };
 struct Art { SDL_Texture* texture=nullptr; int w=0,h=0; unsigned used=0; };
 std::vector<unsigned char> fontData[4];
 std::map<int,Font> fonts;
 std::map<std::string,Art> images;
 unsigned tick=0;
 bool reverse=false;
 float scroll=0;
 // Tab ids keep their v0.3 meaning (0 Games, 1 Settings, 2 About) so the
 // frontend logic that depends on them is unchanged; Map and Controls are new.
 enum TabId { TabGames=0, TabSettings=1, TabAbout=2, TabMap=3, TabControls=4 };
 static constexpr int tabCount=5;
 static const int tabOrder[tabCount]; // visual left-to-right order
 int tab=0;
 int category=0;bool categoryChanged=false;
 enum Action { None, Play, Back, ChangeView, Sort, Previous, Next, Update };
 // Controller state for the Controls tab, written by the frontend every frame.
 enum PadBit : uint32_t { PadA=1u<<0,PadB=1u<<1,PadX=1u<<2,PadY=1u<<3,PadL=1u<<4,PadR=1u<<5,PadZL=1u<<6,PadZR=1u<<7,
  PadPlus=1u<<8,PadMinus=1u<<9,PadUp=1u<<10,PadDown=1u<<11,PadLeft=1u<<12,PadRight=1u<<13,PadLStick=1u<<14,PadRStick=1u<<15 };
 uint32_t padHeld=0; float stickLX=0,stickLY=0,stickRX=0,stickRY=0;
 std::string updateButton="Updates available on Switch",updateStatus="";
 bool updateBusy=false;
 bool pointerDown=false;
 float pointerX=0,pointerY=0;
 SDL_FingerID activeFinger=0;
 bool dragCandidate=false,dragging=false,freeScroll=false,tapBlocked=false;
 float dragStartScroll=0,lastPointerX=0,scrollVelocity=0;
 Uint32 pointerTime=0,scrollTime=0;
 size_t scrollSelection=0;
 void advanceScroll(size_t count,size_t& selected,float dt);
 Action handlePointer(const SDL_Event& event,size_t count,size_t& selected,bool& cartridge);
 explicit LibraryUI(SDL_Renderer* renderer);
 ~LibraryUI();
 void stopStorageScan();
 LibraryUI(const LibraryUI&)=delete;
 LibraryUI& operator=(const LibraryUI&)=delete;
 void cycleTab(int direction);
 static int tabPosition(int id);
 void text(std::string value,int x,int y,int size,SDL_Color color,int maxWidth=1200,bool centered=false,int family=Body);
 int measure(const std::string& value,int size,int family=Body);
 void round(int x,int y,int w,int h,int radius,SDL_Color color);
 void gradient(int x,int y,int w,int h,int radius,SDL_Color top,SDL_Color bottom);
 void circle(int cx,int cy,int radius,SDL_Color color);
 Art& art(const std::string& path);
 void picture(Art& a,int x,int y,int w,int h,int radius);
 void draw(const std::vector<std::string>& games,size_t& selected,bool cartridge,const std::string& status,bool present=true);

 // Map tab: one dot per release in the bundled No-Intro tables, loaded on first use.
 struct MapBlock { int platform=0; int known=0; int owned=0; int x=0,w=0; std::unordered_map<std::string,int> index; std::vector<SDL_Rect> dots; };
 std::vector<MapBlock> mapBlocks;
 int mapState=0; // 0 not loaded, 1 loading on the next frame, 2 ready
 int mapKnown=0,mapCell=3,mapTop=0,mapHeight=0;
 std::string mapSignature;
 std::vector<std::pair<int,int>> mapOwned; // (block, dot) for games in the current shelf
 int mapSelectedBlock=-1,mapSelectedDot=-1;

 // About tab: SD card breakdown, measured once per session on a background thread.
 struct StorageItem { std::string name; uintmax_t bytes=0; SDL_Color color{255,255,255,255}; };
 std::vector<StorageItem> storage;
 std::mutex storageMutex;
 std::atomic<int> storageState{0}; // 0 idle, 1 measuring, 2 done
 std::atomic<bool> storageStop{false}; // set by ~LibraryUI so exit/launch never waits for an SD walk
 SDL_Thread* storageThread=nullptr;
 std::string storageRoot;
private:
 Font* font(int family,int size);
 void drawHeader(int count);
 void drawTabs(bool cartridge);
 void drawGames(const std::vector<std::string>& games,size_t& selected,bool cartridge,const std::string& status);
 void drawMap(const std::vector<std::string>& games,size_t selected);
 void drawControls();
 void drawSettings(bool cartridge);
 void drawAbout();
 void drawFooter();
 void loadMap();
 void layoutMap();
 void arrow(int cx,int cy,int size,int direction,SDL_Color color);
 void padButton(int cx,int cy,int radius,const char* label,uint32_t bit,bool accent=false);
 void padPill(int x,int y,int w,int h,const char* label,uint32_t bit);
};
