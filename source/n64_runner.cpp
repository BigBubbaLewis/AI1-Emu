// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Dedicated hardware-rendered N64 runner; existing software core runner is unchanged.
#include <switch.h>
#include <SDL.h>
#include <libretro.h>
#include "platform_library.h"
#include "safe_file.h"
#include "n64_video.h"
#include "safe_file.h"
#include "game_session.h"
#include "safe_file.h"
#include "cpu_boost.h"
#include "safe_file.h"
#include <memory>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
extern "C" int ai1_n64_prepare_jit(void);
extern "C" void ai1_n64_release_jit(void);
namespace fs=std::filesystem;
static N64Video graphics;static retro_hw_render_callback hardware{};static bool jit=false;static SDL_AudioDeviceID audio;static PadState padState,abortPad;
static uint64_t held;static HidAnalogStickState sticks[2];static unsigned pixel=RETRO_PIXEL_FORMAT_0RGB1555;static Platform platform=Platform::Unknown;
static std::string root,systemDir,saveDir,prefix;static std::map<std::string,std::string> options;
static bool quit=false,running=false;static double fps=60,aspect=0;static GameSession* session=nullptr;static std::vector<int16_t> audioScratch;
static std::string logPath,abortError;static Uint64 frameStarted=0,lastAbortPoll=0;
static void logMessage(enum retro_log_level,const char* fmt,...){FILE* f=logPath.empty()?nullptr:fopen(logPath.c_str(),"a");if(!f)return;va_list args;va_start(args,fmt);vfprintf(f,fmt,args);va_end(args);fclose(f);}
static void phase(const char* title){logMessage(RETRO_LOG_INFO,"%llu ms: %s\n",(unsigned long long)SDL_GetTicks64(),title);graphics.status(title);}
extern "C" bool ai1_n64_should_abort(){
 if(quit)return true;Uint64 now=SDL_GetTicks64();if(now-lastAbortPoll<16)return false;lastAbortPoll=now;
 // Separate state keeps these checks from consuming save/load button edges.
 padUpdate(&abortPad);auto abortHeld=padGetButtons(&abortPad);
 if((!running&&(abortHeld&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus))||!appletMainLoop()){quit=true;return true;}
 if(frameStarted&&now-frameStarted>15000){abortError="N64 frame timed out. See logs/core-n64.log.";quit=true;logMessage(RETRO_LOG_ERROR,"Frame exceeded fifteen seconds; cooperative abort requested\n");return true;}
 return false;
}
static void definitions(const retro_core_option_definition* defs){if(defs)for(auto* d=defs;d->key;++d)if(d->default_value)options[d->key]=d->default_value;}
static void definitions2(const retro_core_option_v2_definition* defs){if(defs)for(auto* d=defs;d->key;++d)if(d->default_value)options[d->key]=d->default_value;}
static bool environment(unsigned command,void* data){switch(command){
 case RETRO_ENVIRONMENT_SET_HW_RENDER:{auto* h=static_cast<retro_hw_render_callback*>(data);if(h->context_type!=RETRO_HW_CONTEXT_OPENGL&&h->context_type!=RETRO_HW_CONTEXT_OPENGL_CORE)return false;h->get_current_framebuffer=[]()->uintptr_t{return graphics.framebuffer();};h->get_proc_address=N64Video::procedure;hardware=*h;return true;}
 case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:static_cast<retro_log_callback*>(data)->log=logMessage;return true;
 case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(data)=2;return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:definitions(static_cast<retro_core_option_definition*>(data));return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:definitions(static_cast<retro_core_options_intl*>(data)->us);return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:definitions2(static_cast<retro_core_options_v2*>(data)->definitions);return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:definitions2(static_cast<retro_core_options_v2_intl*>(data)->us->definitions);return true;
 case RETRO_ENVIRONMENT_SET_VARIABLES:{auto* v=static_cast<retro_variable*>(data);for(;v&&v->key;++v){std::string value=v->value?v->value:"";auto at=value.find(';');if(at!=std::string::npos){value=value.substr(at+1);while(!value.empty()&&value.front()==' ')value.erase(0,1);options[v->key]=value.substr(0,value.find('|'));}}return true;}
 case RETRO_ENVIRONMENT_GET_VARIABLE:{auto* v=static_cast<retro_variable*>(data);const char* fixed=nullptr;
  if(!strcmp(v->key,"mupen64plus-cpucore"))fixed=jit?"dynamic_recompiler":"cached_interpreter";
  else if(!strcmp(v->key,"mupen64plus-rdp-plugin"))fixed="gliden64";
  else if(!strcmp(v->key,"mupen64plus-rsp-plugin"))fixed="hle";
  else if(!strcmp(v->key,"mupen64plus-43screensize"))fixed="320x240";
  else if(!strcmp(v->key,"mupen64plus-alt-map"))fixed="False";
  else if(!strcmp(v->key,"mupen64plus-pak1"))fixed="memory";
  else if(!strcmp(v->key,"mupen64plus-ThreadedRenderer"))fixed="False";
  if(fixed){v->value=fixed;return true;}auto it=options.find(v->key);v->value=it==options.end()?nullptr:it->second.c_str();return v->value!=nullptr;}
 case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*static_cast<bool*>(data)=false;return true;
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:*static_cast<const char**>(data)=systemDir.c_str();return true;
 case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*static_cast<const char**>(data)=saveDir.c_str();return true;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE:*static_cast<bool*>(data)=true;return true;
 case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:*static_cast<int*>(data)=3;return true;
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:pixel=*static_cast<unsigned*>(data);return pixel<=RETRO_PIXEL_FORMAT_RGB565;
 case RETRO_ENVIRONMENT_SET_GEOMETRY:aspect=static_cast<retro_game_geometry*>(data)->aspect_ratio;return true;
 case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:{auto* av=static_cast<retro_system_av_info*>(data);aspect=av->geometry.aspect_ratio;fps=av->timing.fps;return true;}
 case RETRO_ENVIRONMENT_SHUTDOWN:quit=true;return true;
 default:return false;
}}
static void video(const void* data,unsigned w,unsigned h,size_t){if((data==RETRO_HW_FRAME_BUFFER_VALID||!data)&&w&&h){if(!graphics.present(w,h,hardware.bottom_left_origin,float(aspect>0?aspect:4.0/3.0))){abortError="Unsupported N64 framebuffer size.";quit=true;}}}
static size_t samples(const int16_t* data,size_t n){float gain=session?session->audioScale():1.f;if(audio&&gain>0){if(SDL_GetQueuedAudioSize(audio)>96000)SDL_ClearQueuedAudio(audio);if(gain<.999f){audioScratch.assign(data,data+n*2);for(auto& s:audioScratch)s=int16_t(s*gain);SDL_QueueAudio(audio,audioScratch.data(),Uint32(n*4));}else SDL_QueueAudio(audio,data,Uint32(n*4));}return n;}
static void sample(int16_t a,int16_t b){int16_t v[]={a,b};samples(v,1);}
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){if(port||(session&&session->menuOpen()))return 0;
 if(device==RETRO_DEVICE_ANALOG&&index<2){int v=id==RETRO_DEVICE_ID_ANALOG_X?sticks[index].x:-sticks[index].y;return int16_t(std::clamp(v,-32767,32767));}
 if(device!=RETRO_DEVICE_JOYPAD)return 0;
 uint64_t key=0;switch(id){case RETRO_DEVICE_ID_JOYPAD_B:key=HidNpadButton_A;break;case RETRO_DEVICE_ID_JOYPAD_Y:key=HidNpadButton_B;break;case RETRO_DEVICE_ID_JOYPAD_START:key=HidNpadButton_Plus;break;case RETRO_DEVICE_ID_JOYPAD_UP:key=HidNpadButton_Up;break;case RETRO_DEVICE_ID_JOYPAD_DOWN:key=HidNpadButton_Down;break;case RETRO_DEVICE_ID_JOYPAD_LEFT:key=HidNpadButton_Left;break;case RETRO_DEVICE_ID_JOYPAD_RIGHT:key=HidNpadButton_Right;break;case RETRO_DEVICE_ID_JOYPAD_L:key=HidNpadButton_L;break;case RETRO_DEVICE_ID_JOYPAD_R:key=HidNpadButton_R;break;case RETRO_DEVICE_ID_JOYPAD_L2:if(!(held&HidNpadButton_Minus))key=HidNpadButton_ZL;break;}return key&&(held&key)?1:0;}
static std::vector<char> read(const std::string& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)return {};auto n=f.tellg();if(n<=0||n>64LL*1024*1024)return {};std::vector<char> out(static_cast<size_t>(n));f.seekg(0);f.read(out.data(),out.size());return f?std::move(out):std::vector<char>{};}
static std::vector<char> readRom(const std::string& p){
 std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f){abortError="ROM could not be opened.";return {};}
 auto n=f.tellg();if(n<4096||n>128LL*1024*1024){abortError="ROM size is invalid or unsupported.";return {};}
 std::vector<char> out;try{out.resize(static_cast<size_t>(n));}catch(const std::bad_alloc&){abortError="Not enough memory. Use Homebrew Menu application mode.";return {};}
 f.seekg(0);for(size_t at=0;at<out.size();){if(ai1_n64_should_abort())return {};size_t count=std::min(size_t(1024*1024),out.size()-at);if(!f.read(out.data()+at,count)){abortError="SD card ROM read failed.";return {};}at+=count;}
 const unsigned char* bytes=reinterpret_cast<const unsigned char*>(out.data());bool header=(bytes[0]==0x80&&bytes[1]==0x37&&bytes[2]==0x12&&bytes[3]==0x40)||(bytes[0]==0x37&&bytes[1]==0x80&&bytes[2]==0x40&&bytes[3]==0x12)||(bytes[0]==0x40&&bytes[1]==0x12&&bytes[2]==0x37&&bytes[3]==0x80);if(!header){abortError="Invalid N64 cartridge header. Use an uncompressed z64, v64 or n64 file.";return {};}
 logMessage(RETRO_LOG_INFO,"ROM read complete: %zu bytes\n",out.size());return out;
}
// Swap in via safeWriteFile: a plain rename onto an existing save fails on the Switch.
static bool write(const std::string& p,const void* data,size_t n){return safeWriteFile(p,data,n);}
static void returnToLibrary(const std::string& message){if(!message.empty()){std::ofstream f(root+"launch-error.txt");f<<message;}envSetNextLoad("sdmc:/switch/ai1emu/AI1Emu.nro","sdmc:/switch/ai1emu/AI1Emu.nro");}
int main(int argc,char** argv){if(argc<4)return 1;std::string game=argv[1];root=argv[2];platform=static_cast<Platform>(std::atoi(argv[3]));bool resume=argc>4&&std::string(argv[4])=="resume";std::string cover=argc>5?argv[5]:"";prefix=gameSavePrefix(root,game);systemDir=root+"system/"+platformInfo(platform).slug;
 // The core exposes cartridge SRAM and controller pak bytes together.
 // gameSavePrefix isolates them by console and relative ROM path.
 saveDir=root+"saves/n64";
 fs::create_directories(saveDir);fs::create_directories(systemDir);fs::create_directories(fs::path(prefix).parent_path());
 fs::create_directories(root+"logs");logPath=root+"logs/core-"+platformInfo(platform).slug+".log";{std::ofstream log(logPath);log<<"AI1 Emu N64 core launch\nPlatform: "<<platformInfo(platform).slug<<"\nROM: "<<game<<"\nData root: "<<root<<"\nApplet type: "<<int(appletGetAppletType())<<"\n";}
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&padState);padInitializeDefault(&abortPad);hidInitializeTouchScreen();
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){returnToLibrary("Core could not initialize SDL.");return 1;}
 std::string glError;if(!graphics.open(false,glError)){returnToLibrary("N64 OpenGL initialization failed: "+glError);SDL_Quit();return 1;}
 logMessage(RETRO_LOG_INFO,"OpenGL: %s\n",graphics.version());
 if(R_FAILED(romfsInit()))logMessage(RETRO_LOG_WARN,"No RomFS: the menu uses the fallback font\n");jit=ai1_n64_prepare_jit()!=0;logMessage(RETRO_LOG_INFO,"CPU: %s\n",jit?"dynamic recompiler":"cached interpreter (loader denied JIT)");
 phase("INITIALIZING CORE");
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(sample);retro_set_audio_sample_batch(samples);retro_set_input_poll([]{});retro_set_input_state(input);retro_init();retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
 retro_system_info coreInfo{};retro_get_system_info(&coreInfo);std::vector<char> rom;phase("READING ROM");if(!coreInfo.need_fullpath)rom=readRom(game);retro_game_info info{game.c_str(),rom.empty()?nullptr:rom.data(),rom.size(),nullptr};
 phase("LOADING GAME");bool loaded=false;try{loaded=!quit&&abortError.empty()&&(coreInfo.need_fullpath||!rom.empty())&&retro_load_game(&info);}catch(const std::bad_alloc&){abortError="Not enough memory to load this game. Use full-memory application mode.";}catch(const std::exception& e){abortError="Core startup failed. See its log.";logMessage(RETRO_LOG_ERROR,"Exception: %s\n",e.what());}std::string failure;
 if(loaded){if(!hardware.context_reset){abortError="N64 core did not request hardware rendering.";retro_unload_game();loaded=false;}else hardware.context_reset();}
 if(loaded){auto ram=read(newestSave(prefix+".sav"));bool saveCompatible=!fs::exists(prefix+".sav")||(!ram.empty()&&ram.size()==retro_get_memory_size(RETRO_MEMORY_SAVE_RAM));if(!saveCompatible)failure="Existing N64 save has an incompatible size; preserved without overwriting.";size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void* data=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(data&&!ram.empty()&&ram.size()==n)memcpy(data,ram.data(),ram.size());retro_system_av_info av{};retro_get_system_av_info(&av);aspect=av.geometry.aspect_ratio;fps=av.timing.fps>1?av.timing.fps:60;SDL_AudioSpec audioSpec{};audioSpec.freq=int(av.timing.sample_rate);audioSpec.format=AUDIO_S16SYS;audioSpec.channels=2;audioSpec.samples=1024;audio=SDL_OpenAudioDevice(nullptr,0,&audioSpec,nullptr,0);if(audio)SDL_PauseAudioDevice(audio,0);else {failure="N64 audio output could not be opened. See logs/core-n64.log.";logMessage(RETRO_LOG_ERROR,"SDL audio: %s\n",SDL_GetError());}
 auto persist=[&]{size_t size=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void* ptr=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(saveCompatible&&ptr&&size&&!write(prefix+".sav",ptr,size))failure="Game save write failed. Check free SD space.";};
  CoreHooks hooks;hooks.serializeSize=[]{return retro_serialize_size();};hooks.serialize=[](void* d,size_t s){return retro_serialize(d,s);};hooks.unserialize=[](const void* d,size_t s){return retro_unserialize(d,s);};hooks.reset=[]{retro_reset();};hooks.cheatReset=[]{retro_cheat_reset();};hooks.cheatSet=[](unsigned i,bool on,const char* c){retro_cheat_set(i,on,c);};
 // While the menu is open the GL framebuffer shows the menu, so pictures come from a copy taken when it opened.
 std::vector<Uint32> pausedFrame;int pausedW=0,pausedH=0;
 hooks.grabFrame=[&](std::vector<Uint32>& argb,int& w,int& h){if(session&&session->menuOpen()){if(pausedFrame.empty())return false;argb=pausedFrame;w=pausedW;h=pausedH;return true;}return graphics.grab(argb,w,h);};
 static CpuBoost boost;SessionOptions opt;opt.n64=true;opt.rewind=false;opt.looks=false;opt.setBoost=[](bool on){boost.set(on);};
 GameSession menu(graphics.overlayRenderer(),root,game,platform,prefix,cover,hooks,opt);session=&menu;running=true;
 graphics.stretch=menu.gameplay().screen==2;
 if(resume)menu.loadSlot(0,true);
 SDL_Texture* pausedTexture=nullptr;bool wasOpen=false;
 auto drawPaused=[&]{SDL_Renderer* r=graphics.overlayRenderer();SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);if(!pausedTexture)return;float ratio=float(aspect>0?aspect:4.0/3.0);int w=graphics.stretch?1280:std::min(1280,int(720*ratio)),h=graphics.stretch?720:int(w/ratio);SDL_Rect dst{(1280-w)/2,(720-h)/2,w,h};SDL_RenderCopy(r,pausedTexture,nullptr,&dst);};
 auto stamp=std::to_string(std::time(nullptr));write(prefix+".lastplayed",stamp.data(),stamp.size());double next=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();unsigned frames=0;double inputTime=next;uint64_t lastHeld=0;
 while(appletMainLoop()&&!quit){double tick=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();float cursorStep=float(std::min(.05,tick-inputTime))*150.f;inputTime=tick;padUpdate(&padState);held=padGetButtons(&padState);uint64_t down=held&~lastHeld;lastHeld=held;
  bool cornerTap=false;SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)quit=true;if(e.type==SDL_FINGERDOWN&&e.tfinger.x>.91f&&e.tfinger.y<.17f)cornerTap=true;}
  static const std::pair<uint64_t,uint32_t> keys[]={{HidNpadButton_A,pad::A},{HidNpadButton_B,pad::B},{HidNpadButton_X,pad::X},{HidNpadButton_Y,pad::Y},{HidNpadButton_L,pad::L},{HidNpadButton_R,pad::R},{HidNpadButton_ZL,pad::ZL},{HidNpadButton_ZR,pad::ZR},{HidNpadButton_Plus,pad::Plus},{HidNpadButton_Minus,pad::Minus},{HidNpadButton_Up,pad::Up},{HidNpadButton_Down,pad::Down},{HidNpadButton_Left,pad::Left},{HidNpadButton_Right,pad::Right},{HidNpadButton_StickL,pad::LStick},{HidNpadButton_StickR,pad::RStick}};
  uint32_t heldBits=0,downBits=0;for(auto& k:keys){if(held&k.first)heldBits|=k.second;if(down&k.first)downBits|=k.second;}
  boost.keep();if(!menu.update(heldBits,downBits,cornerTap))break;
  if(menu.menuOpen()){if(!wasOpen){wasOpen=true;if(audio)SDL_ClearQueuedAudio(audio);pausedFrame.clear();if(graphics.grab(pausedFrame,pausedW,pausedH)){if(pausedTexture)SDL_DestroyTexture(pausedTexture);pausedTexture=SDL_CreateTexture(graphics.overlayRenderer(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,pausedW,pausedH);if(pausedTexture)SDL_UpdateTexture(pausedTexture,nullptr,pausedFrame.data(),pausedW*4);}}menu.drawMenu(drawPaused);graphics.showOverlay();SDL_Delay(8);next=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();continue;}
  if(wasOpen){wasOpen=false;graphics.stretch=menu.gameplay().screen==2;}
  sticks[0]=padGetStickPos(&padState,0);sticks[1]=padGetStickPos(&padState,1);


  double now=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();if(now<next){SDL_Delay(1);continue;}int count=menu.framesThisLoop();for(int i=0;i<count;++i){frameStarted=SDL_GetTicks64();retro_run();frameStarted=0;menu.afterFrame();if(frames<2)logMessage(RETRO_LOG_INFO,"Frame %u returned\n",frames);if(++frames%300==0)persist();}next+=1.0/fps;if(now-next>.15)next=now+1.0/fps;
 }menu.finish(true);boost.set(false);session=nullptr;running=false;if(pausedTexture)SDL_DestroyTexture(pausedTexture);persist();retro_unload_game();}else if(!quit)failure="Game could not be loaded. Check firmware and logs/core-"+std::string(platformInfo(platform).slug)+".log";
 if(!abortError.empty())failure=abortError;logMessage(RETRO_LOG_INFO,"Exiting core: %s\n",failure.c_str());
 if(hardware.context_destroy)hardware.context_destroy();retro_deinit();ai1_n64_release_jit();if(audio)SDL_CloseAudioDevice(audio);graphics.close();SDL_Quit();romfsExit();returnToLibrary(failure);return loaded?0:1;
}

