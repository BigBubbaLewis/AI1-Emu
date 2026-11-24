#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "library_ui.h"
#include "game_database.h"
#include "platform_library.h"
#include "runtime_paths.h"
#include "app_version.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdio>
namespace fs=std::filesystem;
static bool screenshot(SDL_Renderer* r,const std::string& name){
 SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);if(!s)return false;
 bool ok=SDL_RenderReadPixels(r,nullptr,s->format->format,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,name.c_str())==0;SDL_FreeSurface(s);return ok;
}
// Keyboard stand-in for the Switch controller on the Controls tab.
static uint32_t keyboardPad(){
 const Uint8* k=SDL_GetKeyboardState(nullptr);uint32_t bits=0;
 const std::pair<SDL_Scancode,uint32_t> keys[]={{SDL_SCANCODE_A,LibraryUI::PadA},{SDL_SCANCODE_B,LibraryUI::PadB},{SDL_SCANCODE_X,LibraryUI::PadX},{SDL_SCANCODE_Y,LibraryUI::PadY},
  {SDL_SCANCODE_L,LibraryUI::PadL},{SDL_SCANCODE_R,LibraryUI::PadR},{SDL_SCANCODE_1,LibraryUI::PadZL},{SDL_SCANCODE_2,LibraryUI::PadZR},{SDL_SCANCODE_EQUALS,LibraryUI::PadPlus},
  {SDL_SCANCODE_MINUS,LibraryUI::PadMinus},{SDL_SCANCODE_UP,LibraryUI::PadUp},{SDL_SCANCODE_DOWN,LibraryUI::PadDown},{SDL_SCANCODE_LEFT,LibraryUI::PadLeft},{SDL_SCANCODE_RIGHT,LibraryUI::PadRight}};
 for(auto& key:keys)if(k[key.first])bits|=key.second;return bits;
}
int main(int argc,char** argv){
 SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)return 1;
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
 SDL_Window* w=SDL_CreateWindow("AI1 Emu v" AI1EMU_VERSION " - actual SDL2 UI preview",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_SHOWN);
 SDL_Renderer* r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!r)r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);if(!r){SDL_Log("%s",SDL_GetError());return 2;}
 SDL_RenderSetLogicalSize(r,1280,720);fs::create_directories("screenshots");
 bool capture=argc>1&&std::string(argv[1])=="--capture";
 std::vector<std::string> games;const auto root=retroverseRoot();
 games=scanLibrary(root);
 // Artwork-only catalogue entries are used when no real ROMs are present.
 // They are paths for display/metadata only; no fake ROM files are created.
 if(games.empty()&&fs::exists(root+"artwork-cache"))for(const auto& entry:fs::directory_iterator(root+"artwork-cache"))if(entry.path().extension()==".png")games.push_back(root+"roms/"+entry.path().stem().string()+".gba");
 std::sort(games.begin(),games.end(),[](const auto& a,const auto& b){return gameTitle(a)<gameTitle(b);});
 auto allGames=games;
 {LibraryUI ui(r);size_t selected=0;for(size_t i=0;i<games.size();++i)if(gameTitle(games[i]).find("Emerald")!=std::string::npos||gameTitle(games[i]).find("Anguna")!=std::string::npos)selected=i;
 bool cartridge=false,running=true;std::string status;unsigned frame=0;int shot=0;
 auto save=[&](const char* name){ui.draw(games,selected,cartridge,status,false);char file[96];std::snprintf(file,sizeof file,"screenshots/%02d-%s.bmp",++shot,name);return screenshot(r,file);};
 auto sortGames=[&]{ui.freeScroll=false;ui.scrollVelocity=0;std::string keep=games.empty()?"":games[selected];std::reverse(games.begin(),games.end());ui.reverse=!ui.reverse;if(!games.empty())selected=std::find(games.begin(),games.end(),keep)-games.begin();ui.scroll=0;};
 while(running){
  SDL_Event e;while(SDL_PollEvent(&e)){
   if(e.type==SDL_QUIT)running=false;
   auto touch=ui.handlePointer(e,games.size(),selected,cartridge);
   if(touch==LibraryUI::Back){if(ui.tab)ui.tab=0;else running=false;}
   if(touch==LibraryUI::ChangeView)cartridge=!cartridge;
   if(touch==LibraryUI::Sort)sortGames();
   if(!games.empty()){if(touch==LibraryUI::Next)selected=(selected+1)%games.size();if(touch==LibraryUI::Previous)selected=(selected+games.size()-1)%games.size();}
   if(touch==LibraryUI::Update)ui.updateStatus="Use Settings on your Switch to install updates.";
   if(touch==LibraryUI::Play&&ui.tab==0&&!games.empty())status="Windows UI preview - gameplay is Switch-only";
   if(ui.categoryChanged){games=filterLibrary(allGames,ui.category);if(ui.reverse)std::reverse(games.begin(),games.end());selected=0;ui.scroll=0;ui.categoryChanged=false;}
   if(e.type!=SDL_KEYDOWN)continue;auto key=e.key.keysym.sym;
   if(ui.tab==LibraryUI::TabControls){if(key==SDLK_ESCAPE||key==SDLK_b)ui.tab=0;continue;} // every other key lights the diagram
   if(ui.tab==0&&(key==SDLK_LEFTBRACKET||key==SDLK_RIGHTBRACKET)){ui.category=(ui.category+(key==SDLK_RIGHTBRACKET?1:platforms().size()))%(platforms().size()+1);games=filterLibrary(allGames,ui.category);if(ui.reverse)std::reverse(games.begin(),games.end());selected=0;ui.scroll=0;ui.freeScroll=false;ui.scrollVelocity=0;}
   if(key==SDLK_ESCAPE||key==SDLK_b){if(ui.tab)ui.tab=0;else running=false;}
   if(key==SDLK_q)ui.cycleTab(-1);if(key==SDLK_e)ui.cycleTab(1);if(key==SDLK_p||key==SDLK_PLUS||key==SDLK_EQUALS)ui.tab=1;
   const bool libraryOrSettings=ui.tab==LibraryUI::TabGames||ui.tab==LibraryUI::TabSettings;
   if(libraryOrSettings&&(key==SDLK_x||(ui.tab==1&&(key==SDLK_RETURN||key==SDLK_a))))cartridge=!cartridge;
   if(libraryOrSettings&&key==SDLK_y)sortGames();
   if((ui.tab==0||ui.tab==LibraryUI::TabMap)&&!games.empty()){if(key==SDLK_RIGHT||key==SDLK_DOWN)selected=(selected+1)%games.size();if(key==SDLK_LEFT||key==SDLK_UP)selected=(selected+games.size()-1)%games.size();if(ui.tab==0&&(key==SDLK_RETURN||key==SDLK_a))status="Windows UI preview - gameplay is Switch-only";}
   if(key==SDLK_F12){ui.draw(games,selected,cartridge,status,false);screenshot(r,"screenshots/manual-"+std::to_string(SDL_GetTicks())+".bmp");}
  }
  ui.padHeld=keyboardPad();ui.stickLX=ui.stickLY=ui.stickRX=ui.stickRY=0;
  if(capture){
   // Scripted tour of every v0.4 screen; each capture is a real SDL2 frame.
   ui.updateButton="Check for updates (ZR)";ui.updateStatus="Updates load in the background when this tab opens on a Switch.";
   static int phase=0;static unsigned at=0;auto after=[&](unsigned n){return frame>=at+n;};
   if(ui.tab==LibraryUI::TabControls){ui.padHeld=LibraryUI::PadA|LibraryUI::PadL|LibraryUI::PadUp;ui.stickRX=.85f;ui.stickRY=-.3f;}
   switch(phase){
    case 0:if(after(45)){if(!save("games-cover"))return 3;cartridge=true;phase=1;at=frame;}break;
    case 1:if(after(30)){if(!save("games-cartridge"))return 3;cartridge=false;ui.tab=LibraryUI::TabMap;phase=2;at=frame;}break;
    case 2:if(after(30)&&ui.mapState==2){if(!save("map"))return 3;ui.tab=LibraryUI::TabControls;phase=3;at=frame;}break;
    case 3:if(after(20)){if(!save("controls"))return 3;ui.tab=LibraryUI::TabSettings;phase=4;at=frame;}break;
    case 4:if(after(20)){if(!save("settings"))return 3;ui.tab=LibraryUI::TabAbout;phase=5;at=frame;}break;
    case 5:if(after(30)&&(ui.storageState==2||after(500))){if(!save("about"))return 3;ui.tab=LibraryUI::TabGames;ui.category=1;games=filterLibrary(allGames,1);selected=0;ui.scroll=0;phase=6;at=frame;}break;
    case 6:if(after(40)){if(!save("games-gba"))return 3;ui.category=0;games.clear();phase=7;at=frame;}break;
    default:if(after(20)){if(!save("games-empty"))return 3;running=false;}break;
   }
  }
  ui.draw(games,selected,cartridge,status,false);
  SDL_RenderPresent(r);
  ++frame;SDL_Delay(1);
  if(capture&&frame>1200)running=false;
 }}
 SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();return 0;
}
