#include <switch.h>
#include <SDL.h>
#include "library_ui.h"
#include "runtime_paths.h"
#include "online_updater.h"
#include "game_database.h"
#include "platform_library.h"
#include "legacy_migration.h"
#include "core_status.h"
#include <ctime>
#include <libretro.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
namespace fs=std::filesystem;
static SDL_Renderer* renderer; static SDL_Texture* frame; static SDL_AudioDeviceID audio;
static PadState pad; static uint64_t held; static unsigned pixel=RETRO_PIXEL_FORMAT_0RGB1555;
static std::string root,systemDirectory,coreSaveDirectory;static double displayAspect=1.5;
static bool environment(unsigned cmd,void* data){
 switch(cmd){
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: pixel=*(unsigned*)data; return pixel==RETRO_PIXEL_FORMAT_RGB565||pixel==RETRO_PIXEL_FORMAT_XRGB8888||pixel==RETRO_PIXEL_FORMAT_0RGB1555;
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char**)data=systemDirectory.c_str();return true;
 case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data=coreSaveDirectory.c_str();return true;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)data=true;return true;
 default:return false;}}
static void video(const void* data,unsigned w,unsigned h,size_t pitch){
 if(!data)return;
 static unsigned lastW=0,lastH=0,lastPixel=99;
 if(w!=lastW||h!=lastH||pixel!=lastPixel){if(frame)SDL_DestroyTexture(frame);frame=SDL_CreateTexture(renderer,pixel==RETRO_PIXEL_FORMAT_RGB565?SDL_PIXELFORMAT_RGB565:pixel==RETRO_PIXEL_FORMAT_XRGB8888?SDL_PIXELFORMAT_ARGB8888:SDL_PIXELFORMAT_ARGB1555,SDL_TEXTUREACCESS_STREAMING,w,h);lastW=w;lastH=h;lastPixel=pixel;}
 if(frame){SDL_UpdateTexture(frame,nullptr,data,pitch);SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);double ratio=displayAspect>0?displayAspect:double(w)/h;int width=std::min(960,int(640*ratio)),height=int(width/ratio);SDL_Rect dst{160+(960-width)/2,40+(640-height)/2,width,height};SDL_RenderCopy(renderer,frame,nullptr,&dst);SDL_RenderPresent(renderer);}}
static size_t samples(const int16_t* data,size_t n){if(audio){if(SDL_GetQueuedAudioSize(audio)>48000)SDL_ClearQueuedAudio(audio);SDL_QueueAudio(audio,data,n*4);}return n;}
static void sample(int16_t l,int16_t r){int16_t v[]={l,r};samples(v,1);}
static int16_t input(unsigned port,unsigned device,unsigned,unsigned id){if(port||device!=RETRO_DEVICE_JOYPAD)return 0;static const uint64_t keys[]={HidNpadButton_B,HidNpadButton_Y,HidNpadButton_Minus,HidNpadButton_Plus,HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_Left,HidNpadButton_Right,HidNpadButton_A,HidNpadButton_X,HidNpadButton_L,HidNpadButton_R};return id<12&&(held&keys[id])?1:0;}
static std::vector<char> read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static bool write(const std::string& p,const void* d,size_t n){std::ofstream f(p+".tmp",std::ios::binary);f.write((const char*)d,n);f.close();if(!f)return false;return std::rename((p+".tmp").c_str(),p.c_str())==0;}
static void rect(int x,int y,int w,int h,int r,int g,int b){SDL_SetRenderDrawColor(renderer,r,g,b,255);SDL_Rect a{x,y,w,h};SDL_RenderFillRect(renderer,&a);}
int main(){
 OnlineUpdater updater;bool installUpdate=false,checkedUpdates=false;
 if(R_FAILED(romfsInit()))return 3;
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);hidInitializeTouchScreen();
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0)return 1;
 SDL_Window* window=SDL_CreateWindow("AI1 Emu",0,0,1280,720,0);
 renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer)return 2;
 SDL_RenderSetLogicalSize(renderer,1280,720);
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
 Uint32 lastMigrationDraw=0;
 auto migration=mergeLegacyFolder("sdmc:/switch",[&](const std::string& message){
  padUpdate(&pad);auto buttons=padGetButtons(&pad);if((buttons&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus))return false;
  if(SDL_GetTicks()-lastMigrationDraw>150){coreStatus(renderer,message.c_str());lastMigrationDraw=SDL_GetTicks();}return appletMainLoop();
 });
 root=retroverseRoot();systemDirectory=root;coreSaveDirectory=root+"saves";
 fs::create_directories(root+"roms");fs::create_directories(root+"saves");
 for(const char* dir:{"covers","screenshots","metadata"})fs::create_directories(root+dir);
 for(auto& info:platforms()){fs::create_directories(root+"roms/"+info.slug);fs::create_directories(root+"saves/"+info.slug);fs::create_directories(root+"system/"+info.slug);}
 { LibraryUI ui(renderer);
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(sample);retro_set_audio_sample_batch(samples);retro_set_input_poll([]{});retro_set_input_state(input);retro_init();retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
 std::vector<std::string> allGames=scanLibrary(root);std::sort(allGames.begin(),allGames.end(),[](const auto& a,const auto& b){return gameTitle(a)<gameTitle(b);});std::vector<std::string> games=allGames;
 size_t selected=0;bool playing=false,cartridge=false;std::vector<char> rom;std::string save,status=migration.message;
 {std::ifstream errors(root+"launch-error.txt");if(errors){std::getline(errors,status);errors.close();std::remove((root+"launch-error.txt").c_str());}}
 {std::ifstream session(root+"library-session.txt");int cat=0,view=0;std::string path;if(session>>cat>>view){session.ignore();std::getline(session,path);ui.category=std::clamp(cat,0,int(platforms().size()));cartridge=view!=0;games=filterLibrary(allGames,ui.category);auto found=std::find(games.begin(),games.end(),path);if(found!=games.end())selected=found-games.begin();session.close();std::remove((root+"library-session.txt").c_str());}}
 auto persist=[&]{size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void* p=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(n&&p&&!write(save+".sav",p,n))status="SAVE WRITE FAILED";};
 while(appletMainLoop()){
 padUpdate(&pad);held=padGetButtons(&pad);auto down=padGetButtonsDown(&pad);{uint32_t bits=0;const std::pair<uint64_t,uint32_t> keys[]={{HidNpadButton_A,LibraryUI::PadA},{HidNpadButton_B,LibraryUI::PadB},{HidNpadButton_X,LibraryUI::PadX},{HidNpadButton_Y,LibraryUI::PadY},{HidNpadButton_L,LibraryUI::PadL},{HidNpadButton_R,LibraryUI::PadR},{HidNpadButton_ZL,LibraryUI::PadZL},{HidNpadButton_ZR,LibraryUI::PadZR},{HidNpadButton_Plus,LibraryUI::PadPlus},{HidNpadButton_Minus,LibraryUI::PadMinus},{HidNpadButton_Up,LibraryUI::PadUp},{HidNpadButton_Down,LibraryUI::PadDown},{HidNpadButton_Left,LibraryUI::PadLeft},{HidNpadButton_Right,LibraryUI::PadRight},{HidNpadButton_StickL,LibraryUI::PadLStick},{HidNpadButton_StickR,LibraryUI::PadRStick}};for(auto& key:keys)if(held&key.first)bits|=key.second;ui.padHeld=bits;auto leftStick=padGetStickPos(&pad,0),rightStick=padGetStickPos(&pad,1);ui.stickLX=leftStick.x/32767.f;ui.stickLY=-leftStick.y/32767.f;ui.stickRX=rightStick.x/32767.f;ui.stickRY=-rightStick.y/32767.f;}SDL_Event event;while(SDL_PollEvent(&event)){if(!playing){auto action=ui.handlePointer(event,games.size(),selected,cartridge);switch(action){case LibraryUI::Play:if(ui.tab==0)down|=HidNpadButton_A;break;case LibraryUI::Back:down|=HidNpadButton_B;break;case LibraryUI::ChangeView:down|=HidNpadButton_X;break;case LibraryUI::Sort:down|=HidNpadButton_Y;break;case LibraryUI::Previous:down|=HidNpadButton_Left;break;case LibraryUI::Next:down|=HidNpadButton_Right;break;case LibraryUI::Update:down|=HidNpadButton_ZR;break;default:break;}}}
 if(playing){
 if((held&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus)){persist();retro_unload_game();playing=false;if(audio)SDL_ClearQueuedAudio(audio);continue;}
 if(down&HidNpadButton_ZR){std::vector<char> state(retro_serialize_size());status=!state.empty()&&retro_serialize(state.data(),state.size())&&write(save+".state",state.data(),state.size())?"STATE SAVED":"STATE SAVE FAILED";}
 if(down&HidNpadButton_ZL){auto state=read(save+".state");status=!state.empty()&&retro_unserialize(state.data(),state.size())?"STATE LOADED":"STATE LOAD FAILED";}
 retro_run();static unsigned frames=0;if(++frames%300==0)persist();continue;}
 if(ui.tab==1&&!checkedUpdates){updater.check();checkedUpdates=true;}
 if(ui.tab==1&&(down&HidNpadButton_ZR)){if(updater.phase()==OnlineUpdater::Available)updater.download();else if(updater.phase()==OnlineUpdater::Ready){installUpdate=true;break;}else updater.check();}
 ui.updateButton=updater.label()+" (ZR)";ui.updateStatus=updater.message();ui.updateBusy=updater.phase()==OnlineUpdater::Checking||updater.phase()==OnlineUpdater::Downloading;
 if(ui.tab==0){if(down&HidNpadButton_ZL){ui.category=(ui.category+platforms().size())%(platforms().size()+1);ui.categoryChanged=true;}if(down&HidNpadButton_ZR){ui.category=(ui.category+1)%(platforms().size()+1);ui.categoryChanged=true;}}
 if(ui.categoryChanged){games=filterLibrary(allGames,ui.category);if(ui.reverse)std::reverse(games.begin(),games.end());selected=0;ui.scroll=0;ui.freeScroll=false;ui.scrollVelocity=0;ui.categoryChanged=false;}
 auto stick=padGetStickPos(&pad,0);
 uint64_t nav=held&(HidNpadButton_Left|HidNpadButton_Right);if(stick.x>16000)nav|=HidNpadButton_Right;if(stick.x< -16000)nav|=HidNpadButton_Left;
 static uint64_t lastNav=0;static Uint32 nextNav=0;Uint32 now=SDL_GetTicks();if(nav!=lastNav){down|=nav;nextNav=now+300;}else if(nav&&Sint32(now-nextNav)>=0){down|=nav;nextNav=now+110;}lastNav=nav;
 // Controls tab: every button lights the diagram; only B leaves (touch tabs also work).
 if(ui.tab==LibraryUI::TabControls){if(down&HidNpadButton_B)ui.tab=LibraryUI::TabGames;ui.draw(games,selected,cartridge,status);continue;}
 if(down&HidNpadButton_B){if(ui.tab)ui.tab=0;else break;}
 if(down&HidNpadButton_L)ui.cycleTab(-1);
 if(down&HidNpadButton_R)ui.cycleTab(1);
 if(down&(HidNpadButton_Plus|HidNpadButton_Minus))ui.tab=1;
 const bool libraryOrSettings=ui.tab==LibraryUI::TabGames||ui.tab==LibraryUI::TabSettings;
 if(libraryOrSettings&&(down&HidNpadButton_Y)){ui.freeScroll=false;ui.scrollVelocity=0;std::string keep=games.empty()?"":games[selected];ui.reverse=!ui.reverse;std::reverse(games.begin(),games.end());if(!games.empty())selected=std::find(games.begin(),games.end(),keep)-games.begin();ui.scroll=0;}
 if(ui.tab==1&&(down&HidNpadButton_A))cartridge=!cartridge;
 if(libraryOrSettings&&(down&HidNpadButton_X))cartridge=!cartridge;
 if((ui.tab==LibraryUI::TabGames||ui.tab==LibraryUI::TabMap)&&!games.empty()){
 if(down&(HidNpadButton_Right|HidNpadButton_Down))selected=(selected+1)%games.size();
 if(down&(HidNpadButton_Left|HidNpadButton_Up))selected=(selected+games.size()-1)%games.size();
 if(ui.tab==LibraryUI::TabGames&&(down&HidNpadButton_A)&&updater.phase()!=OnlineUpdater::Downloading){
 auto platform=detectPlatform(games[selected]);const auto& infoPlatform=platformInfo(platform);
 if((isDualScreen(platform)||platform==Platform::N64)&&appletGetAppletType()!=AppletType_Application&&appletGetAppletType()!=AppletType_SystemApplication){status="This core needs full memory. Launch the AI1 HOME tile or hold R while opening a game.";continue;}
 if(infoPlatform.core[0]){
  std::string corePath="sdmc:/switch/ai1emu/cores/"+std::string(infoPlatform.core);
  if(!fs::exists(corePath)){status="Missing core. For N64, open Settings and check for updates; otherwise install the complete package.";continue;}
  if(!envHasNextLoad()){status="This loader cannot chainload cores. Use Homebrew Menu application mode.";continue;}
  if(games[selected].find_first_of("\"\r\n")!=std::string::npos){status="Rename this ROM to remove quotes or line breaks.";continue;}
  std::string args=corePath+" \""+games[selected]+"\" \""+root+"\" "+std::to_string(int(platform));
  if(R_SUCCEEDED(envSetNextLoad(corePath.c_str(),args.c_str()))){std::ofstream session(root+"library-session.txt");session<<ui.category<<" "<<cartridge<<"\n"<<games[selected];break;}
  status="Could not launch the selected core.";continue;
 }
 ui.stopStorageScan();
 systemDirectory=root+"system/"+infoPlatform.slug;coreSaveDirectory=root+"saves/"+infoPlatform.slug;
 if(platform==Platform::GBA&&fs::exists(root+"gba_bios.bin"))systemDirectory=root;
 rom=read(games[selected]);retro_game_info info{};info.path=games[selected].c_str();info.data=rom.data();info.size=rom.size();if(retro_load_game(&info)){save=gameSavePrefix(root,games[selected]);fs::create_directories(fs::path(save).parent_path());auto ram=read(save+".sav");auto n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);if(!ram.empty()&&ram.size()<=n&&retro_get_memory_data(RETRO_MEMORY_SAVE_RAM))memcpy(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM),ram.data(),ram.size());retro_system_av_info av{};retro_get_system_av_info(&av);displayAspect=av.geometry.aspect_ratio>0?av.geometry.aspect_ratio:double(av.geometry.base_width)/av.geometry.base_height;if(audio)SDL_CloseAudioDevice(audio);SDL_AudioSpec spec{};spec.freq=(int)av.timing.sample_rate;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=1024;audio=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);if(audio)SDL_PauseAudioDevice(audio,0);playing=true;auto stamp=std::to_string(std::time(nullptr));write(save+".lastplayed",stamp.data(),stamp.size());}else status="ROM LOAD FAILED";}}
 ui.draw(games,selected,cartridge,status);
 }
 if(playing){persist();retro_unload_game();}retro_deinit();if(audio)SDL_CloseAudioDevice(audio);if(frame)SDL_DestroyTexture(frame);} SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();romfsExit();if(installUpdate&&!updater.apply()){std::ofstream error("sdmc:/switch/ai1emu/update-error.txt");error<<"Update could not be installed; the previous app was retained. Retry from Settings.";}return 0;
}












