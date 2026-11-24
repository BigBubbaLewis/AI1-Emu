// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Built once per core; cores with different licenses are never linked together.
#include <switch.h>
#include <SDL.h>
#include <libretro.h>
#include "platform_library.h"
#include "dual_screen.h"
#include "core_status.h"
#include "ds_audio.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
namespace fs=std::filesystem;
static SDL_Renderer* renderer;static SDL_Texture* texture;static SDL_AudioDeviceID audio;static PadState pad,abortPad;
static unsigned audioRate=32768;static DSAudioQueue audioQueue;
static uint64_t held;static unsigned pixel=RETRO_PIXEL_FORMAT_0RGB1555;static Platform platform=Platform::Unknown;
static std::string root,systemDir,saveDir,prefix;static std::map<std::string,std::string> options;
static bool quit=false,touch=false;static float touchX=128,touchY=96;static int layout=0;static double fps=60,aspect=0;
static std::string logPath,abortError;static Uint64 frameStarted=0,lastAbortPoll=0;
static void logMessage(enum retro_log_level,const char* fmt,...){FILE* f=logPath.empty()?nullptr:fopen(logPath.c_str(),"a");if(!f)return;va_list args;va_start(args,fmt);vfprintf(f,fmt,args);va_end(args);fclose(f);}
static void phase(const char* title){logMessage(RETRO_LOG_INFO,"%llu ms: %s\n",(unsigned long long)SDL_GetTicks64(),title);coreStatus(renderer,title);}
extern "C" bool ai1CoreAbortRequested(){
 if(quit)return true;Uint64 now=SDL_GetTicks64();if(now-lastAbortPoll<16)return false;lastAbortPoll=now;
 // Separate state keeps these checks from consuming save/load button edges.
 padUpdate(&abortPad);auto abortHeld=padGetButtons(&abortPad);
 if((abortHeld&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus)||!appletMainLoop()){quit=true;return true;}
 if(frameStarted&&now-frameStarted>5000){abortError="DS frame timed out. See logs/core-nds.log (or core-dsi.log).";quit=true;logMessage(RETRO_LOG_ERROR,"Frame exceeded five seconds; cooperative abort requested\n");return true;}
 return false;
}
static void definitions(const retro_core_option_definition* defs){if(defs)for(auto* d=defs;d->key;++d)if(d->default_value)options[d->key]=d->default_value;}
static void definitions2(const retro_core_option_v2_definition* defs){if(defs)for(auto* d=defs;d->key;++d)if(d->default_value)options[d->key]=d->default_value;}
static bool environment(unsigned command,void* data){switch(command){
 case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:static_cast<retro_log_callback*>(data)->log=logMessage;return true;
 case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(data)=2;return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:definitions(static_cast<retro_core_option_definition*>(data));return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:definitions(static_cast<retro_core_options_intl*>(data)->us);return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:definitions2(static_cast<retro_core_options_v2*>(data)->definitions);return true;
 case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:definitions2(static_cast<retro_core_options_v2_intl*>(data)->us->definitions);return true;
 case RETRO_ENVIRONMENT_SET_VARIABLES:{auto* v=static_cast<retro_variable*>(data);for(;v&&v->key;++v){std::string value=v->value?v->value:"";auto at=value.find(';');if(at!=std::string::npos){value=value.substr(at+1);while(!value.empty()&&value.front()==' ')value.erase(0,1);options[v->key]=value.substr(0,value.find('|'));}}return true;}
 case RETRO_ENVIRONMENT_GET_VARIABLE:{auto* v=static_cast<retro_variable*>(data);const char* fixed=nullptr;
  if(!strcmp(v->key,"melonds_screen_layout"))fixed="Top/Bottom";else if(!strcmp(v->key,"melonds_screen_gap"))fixed="0";else if(!strcmp(v->key,"melonds_touch_mode"))fixed="Touch";else if(!strcmp(v->key,"melonds_opengl_renderer"))fixed="disabled";else if(!strcmp(v->key,"melonds_boot_directly"))fixed="enabled";else if(!strcmp(v->key,"melonds_console_mode"))fixed=platform==Platform::DSi?"DSi":"DS";
  else if(!strcmp(v->key,"melonds_threaded_renderer"))fixed="disabled";
  else if(!strcmp(v->key,"genesis_plus_gx_system_hw"))fixed=platform==Platform::GameGear?"game gear":platform==Platform::MasterSystem?"master system":"mega drive / genesis";
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
static void video(const void* data,unsigned w,unsigned h,size_t pitch){if(!data||data==RETRO_HW_FRAME_BUFFER_VALID)return;static unsigned lastW=0,lastH=0,lastPixel=99;if(w!=lastW||h!=lastH||pixel!=lastPixel){if(texture)SDL_DestroyTexture(texture);texture=SDL_CreateTexture(renderer,pixel==RETRO_PIXEL_FORMAT_RGB565?SDL_PIXELFORMAT_RGB565:pixel==RETRO_PIXEL_FORMAT_XRGB8888?SDL_PIXELFORMAT_ARGB8888:SDL_PIXELFORMAT_ARGB1555,SDL_TEXTUREACCESS_STREAMING,w,h);lastW=w;lastH=h;lastPixel=pixel;}if(!texture)return;SDL_UpdateTexture(texture,nullptr,data,int(pitch));SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
 if(isDualScreen(platform)&&h==w*3/2){auto rect=dualScreenLayout(layout);SDL_Rect top{0,0,int(w),int(h/2)},bottom{0,int(h/2),int(w),int(h/2)};SDL_RenderCopy(renderer,texture,&top,&rect.top);SDL_RenderCopy(renderer,texture,&bottom,&rect.bottom);int x=rect.bottom.x+int(touchX*rect.bottom.w/256),y=rect.bottom.y+int(touchY*rect.bottom.h/192);SDL_SetRenderDrawColor(renderer,255,80,90,255);SDL_RenderDrawLine(renderer,x-6,y,x+6,y);SDL_RenderDrawLine(renderer,x,y-6,x,y+6);
 }else{double ratio=aspect>0?aspect:double(w)/h;int width=std::min(1280,int(720*ratio)),height=int(width/ratio);SDL_Rect dst{(1280-width)/2,(720-height)/2,width,height};SDL_RenderCopy(renderer,texture,nullptr,&dst);}SDL_RenderPresent(renderer);}
static size_t samples(const int16_t* data,size_t n){if(!audioQueue.push(data,n,ai1CoreAbortRequested)){logMessage(RETRO_LOG_ERROR,"Audio enqueue stopped: %s\n",SDL_GetError());return 0;}return n;}
static void sample(int16_t a,int16_t b){int16_t v[]={a,b};samples(v,1);}
static int16_t input(unsigned port,unsigned device,unsigned,unsigned id){if(port)return 0;if(device==RETRO_DEVICE_POINTER){if(id==RETRO_DEVICE_ID_POINTER_PRESSED)return touch||(held&HidNpadButton_StickR);if(id==RETRO_DEVICE_ID_POINTER_X)return int16_t(touchX*65536/256-32768);if(id==RETRO_DEVICE_ID_POINTER_Y)return int16_t((192+touchY)*65536/384-32768);return 0;}if(device!=RETRO_DEVICE_JOYPAD)return 0;static const uint64_t keys[]={HidNpadButton_B,HidNpadButton_Y,HidNpadButton_Minus,HidNpadButton_Plus,HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_Left,HidNpadButton_Right,HidNpadButton_A,HidNpadButton_X,HidNpadButton_L,HidNpadButton_R};return id<12&&(held&keys[id])?1:0;}
static std::vector<char> read(const std::string& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)return {};auto n=f.tellg();if(n<=0||n>512LL*1024*1024)return {};std::vector<char> out(static_cast<size_t>(n));f.seekg(0);f.read(out.data(),out.size());return f?std::move(out):std::vector<char>{};}
static std::vector<char> readRom(const std::string& p){
 std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f){abortError="ROM could not be opened.";return {};}
 auto n=f.tellg();if(n<512||n>512LL*1024*1024){abortError="ROM size is invalid or unsupported.";return {};}
 std::vector<char> out;try{out.resize(static_cast<size_t>(n));}catch(const std::bad_alloc&){abortError="Not enough memory. Use Homebrew Menu application mode.";return {};}
 f.seekg(0);for(size_t at=0;at<out.size();){if(ai1CoreAbortRequested())return {};size_t count=std::min(size_t(1024*1024),out.size()-at);if(!f.read(out.data()+at,count)){abortError="SD card ROM read failed.";return {};}at+=count;}
 logMessage(RETRO_LOG_INFO,"ROM read complete: %zu bytes\n",out.size());return out;
}
static bool write(const std::string& p,const void* data,size_t n){std::ofstream f(p+".tmp",std::ios::binary);f.write(static_cast<const char*>(data),n);f.close();return f&&std::rename((p+".tmp").c_str(),p.c_str())==0;}
static void returnToLibrary(const std::string& message){if(!message.empty()){std::ofstream f(root+"launch-error.txt");f<<message;}envSetNextLoad("sdmc:/switch/ai1emu/AI1Emu.nro","sdmc:/switch/ai1emu/AI1Emu.nro");}
int main(int argc,char** argv){if(argc<4)return 1;std::string game=argv[1];root=argv[2];platform=static_cast<Platform>(std::atoi(argv[3]));prefix=gameSavePrefix(root,game);systemDir=root+"system/"+platformInfo(platform).slug;
 // melonDS owns its SRAM internally. A unique per-game save directory prevents
 // collisions even when identical ROM filenames appear in different folders.
 saveDir=isDualScreen(platform)?prefix+".files":root+"saves/"+platformInfo(platform).slug;
 fs::create_directories(saveDir);fs::create_directories(systemDir);fs::create_directories(fs::path(prefix).parent_path());
 fs::create_directories(root+"logs");logPath=root+"logs/core-"+platformInfo(platform).slug+".log";{std::ofstream log(logPath);log<<"AI1 Emu DS buffered-audio runner 1\nPlatform: "<<platformInfo(platform).slug<<"\nROM: "<<game<<"\nData root: "<<root<<"\nApplet type: "<<int(appletGetAppletType())<<"\n";}
 if(platform==Platform::DSi){for(const char* name:{"dsi_bios7.bin","dsi_bios9.bin","dsi_firmware.bin","dsi_nand.bin"})if(!fs::exists(systemDir+"/"+name)){returnToLibrary("DSi requires your own BIOS, firmware and NAND in system/dsi. DSi states are unsupported.");return 1;}}
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);padInitializeDefault(&abortPad);hidInitializeTouchScreen();
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){returnToLibrary("Core could not initialize SDL.");return 1;}
 SDL_Window* window=SDL_CreateWindow("AI1 Emu",0,0,1280,720,0);renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);if(!renderer){returnToLibrary("Core could not create video output.");SDL_Quit();return 1;}SDL_RenderSetLogicalSize(renderer,1280,720);
 phase("INITIALIZING CORE");
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(sample);retro_set_audio_sample_batch(samples);retro_set_input_poll([]{});retro_set_input_state(input);retro_init();retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
 retro_system_info coreInfo{};retro_get_system_info(&coreInfo);std::vector<char> rom;phase("READING ROM");if(!coreInfo.need_fullpath)rom=readRom(game);retro_game_info info{game.c_str(),rom.empty()?nullptr:rom.data(),rom.size(),nullptr};
 phase("LOADING GAME");bool loaded=false;try{loaded=!quit&&abortError.empty()&&(coreInfo.need_fullpath||!rom.empty())&&retro_load_game(&info);}catch(const std::bad_alloc&){abortError="Not enough memory to load this game. Use full-memory application mode.";}catch(const std::exception& e){abortError="Core startup failed. See its log.";logMessage(RETRO_LOG_ERROR,"Exception: %s\n",e.what());}std::string failure;
 if(loaded)phase("STARTING GAME");
 if(loaded){auto ram=read(prefix+".sav");size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void* data=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(data&&!ram.empty()&&ram.size()<=n)memcpy(data,ram.data(),ram.size());retro_system_av_info av{};retro_get_system_av_info(&av);aspect=av.geometry.aspect_ratio;fps=av.timing.fps>1?av.timing.fps:60;SDL_AudioSpec audioSpec{};audioRate=unsigned(av.timing.sample_rate);audioSpec.freq=int(audioRate);audioSpec.format=AUDIO_S16SYS;audioSpec.channels=2;audioSpec.samples=512;audio=SDL_OpenAudioDevice(nullptr,0,&audioSpec,nullptr,0);audioQueue.configure(audio,audioRate);if(!audio)logMessage(RETRO_LOG_ERROR,"SDL audio open failed: %s\n",SDL_GetError());
 auto persist=[&]{size_t size=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void* ptr=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(ptr&&size&&!write(prefix+".sav",ptr,size))failure="Game save write failed. Check free SD space.";};
 auto stamp=std::to_string(std::time(nullptr));write(prefix+".lastplayed",stamp.data(),stamp.size());double next=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();unsigned frames=0;double inputTime=next;
 while(appletMainLoop()&&!quit){double tick=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();float cursorStep=float(std::min(.05,tick-inputTime))*150.f;inputTime=tick;padUpdate(&pad);held=padGetButtons(&pad);auto down=padGetButtonsDown(&pad);if((held&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus))break;
  SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_FINGERUP)touch=false;if(isDualScreen(platform)&&(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERMOTION)){auto rect=dualScreenLayout(layout);touch=bottomScreenPoint(rect.bottom,e.tfinger.x*1280,e.tfinger.y*720,touchX,touchY);}}
  if(isDualScreen(platform)){if(down&HidNpadButton_StickL){layout=(layout+1)%3;touch=false;}auto stick=padGetStickPos(&pad,1);if(std::abs(stick.x)>5000)touchX=std::clamp(touchX+stick.x/32768.f*cursorStep,0.f,255.f);if(std::abs(stick.y)>5000)touchY=std::clamp(touchY-stick.y/32768.f*cursorStep,0.f,191.f);}
  if(down&HidNpadButton_ZR){size_t n=retro_serialize_size();if(n&&n<64*1024*1024){std::vector<char> state(n);if(!retro_serialize(state.data(),state.size())||!write(prefix+".state",state.data(),state.size()))failure="State could not be saved.";}else failure=platform==Platform::DSi?"DSi save states are not supported by this core.":"Save states are unavailable for this game/core.";}
  if(down&HidNpadButton_ZL){auto state=read(prefix+".state");if(state.empty()||!retro_unserialize(state.data(),state.size()))failure="State could not be loaded for this game/core.";else if(audio){audioQueue.reset();}}
  double now=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();if(now<next){SDL_Delay(1);continue;}frameStarted=SDL_GetTicks64();retro_run();frameStarted=0;if(frames<2)logMessage(RETRO_LOG_INFO,"Frame %u returned\n",frames);next+=dsAudioFrameInterval(fps,audio?SDL_GetQueuedAudioSize(audio):0,dsAudioTargetBytes(audioRate),audioQueue.playing);if(now-next>.15)next=now+1.0/fps;if(++frames%300==0){persist();logMessage(RETRO_LOG_INFO,"Audio: queue=%u bytes, underruns=%u, waits=%u\n",audio?SDL_GetQueuedAudioSize(audio):0,audioQueue.underruns,audioQueue.waits);}
 }persist();retro_unload_game();}else if(!quit)failure="Game could not be loaded. Check firmware and logs/core-"+std::string(platformInfo(platform).slug)+".log";
 if(!abortError.empty())failure=abortError;logMessage(RETRO_LOG_INFO,"Exiting core: %s\n",failure.c_str());
 retro_deinit();if(audio)SDL_CloseAudioDevice(audio);if(texture)SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();returnToLibrary(failure);return loaded?0:1;
}

