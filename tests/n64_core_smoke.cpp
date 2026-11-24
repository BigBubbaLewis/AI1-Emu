// SPDX-License-Identifier: MIT
#define SDL_MAIN_HANDLED
#include <windows.h>
#include <libretro.h>
#include "n64_video.h"
#include <map>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cassert>
static N64Video graphics;static retro_hw_render_callback hardware{};static bool jit=false,quit=false;static unsigned pixel=0;static double aspect=4./3,fps=60;static std::map<std::string,std::string> options;static std::string systemDir="build/n64-host-system",saveDir="build/n64-host-saves";
static void logMessage(retro_log_level,const char* fmt,...){va_list args;va_start(args,fmt);vfprintf(stdout,fmt,args);va_end(args);fflush(stdout);}
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

static unsigned videos=0;static size_t audioFrames=0;
static void video(const void* d,unsigned w,unsigned h,size_t){if(d==RETRO_HW_FRAME_BUFFER_VALID){assert(graphics.present(w,h));++videos;}}
static size_t audio(const int16_t*,size_t n){audioFrames+=n;return n;}
static void single(int16_t,int16_t){++audioFrames;}
int main(){SDL_SetMainReady();assert(SDL_Init(SDL_INIT_VIDEO)==0);std::string error;if(!graphics.open(true,error)){fprintf(stderr,"%s\n",error.c_str());return 1;}
HMODULE dll=LoadLibraryA("build/n64-host-source/mupen64plus_next_libretro.dll");if(!dll){printf("DLL error %lu\n",GetLastError());return 2;}
#define API(name) auto name=reinterpret_cast<decltype(&::name)>(GetProcAddress(dll,#name));assert(name)
API(retro_set_controller_port_device);API(retro_set_environment);API(retro_set_video_refresh);API(retro_set_audio_sample);API(retro_set_audio_sample_batch);API(retro_set_input_poll);API(retro_set_input_state);API(retro_init);API(retro_load_game);API(retro_run);API(retro_unload_game);API(retro_deinit);API(retro_serialize_size);API(retro_serialize);API(retro_unserialize);API(retro_get_memory_data);API(retro_get_memory_size);
retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(single);retro_set_audio_sample_batch(audio);retro_set_input_poll([]{});retro_set_input_state([](unsigned,unsigned,unsigned,unsigned)->int16_t{return 0;});retro_init();retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
std::ifstream f("build/n64-diagnostic.z64",std::ios::binary|std::ios::ate);std::vector<char> bytes(size_t(f.tellg()));f.seekg(0);f.read(bytes.data(),bytes.size());retro_game_info game{"build/n64-diagnostic.z64",bytes.data(),bytes.size(),nullptr};assert(retro_load_game(&game));assert(hardware.context_reset);hardware.context_reset();for(int i=0;i<120&&!quit;i++)retro_run();
auto* ram=static_cast<unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));assert(ram);unsigned marker=0;memcpy(&marker,ram+0x1000,4);printf("Marker %08x video %u audio %zu\n",marker,videos,audioFrames);assert(marker==0x41493121);assert(videos>0);assert(audioFrames>0);
size_t n=retro_serialize_size();assert(n>0&&n<64*1024*1024);std::vector<char> state(n);assert(retro_serialize(state.data(),n));memset(ram+0x1000,0,4);assert(retro_unserialize(state.data(),n));memcpy(&marker,ram+0x1000,4);assert(marker==0x41493121);assert(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)>0);assert(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));retro_unload_game();if(hardware.context_destroy)hardware.context_destroy();retro_deinit();graphics.close();FreeLibrary(dll);SDL_Quit();puts("PASS: original MIPS diagnostic, actual core/OpenGL video/audio callbacks, SRAM, state round trip and shutdown");return 0;}
