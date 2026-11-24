#define SDL_MAIN_HANDLED
#include "ds_audio.h"
#include <windows.h>
#include <libretro.h>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <fstream>
static DSAudioQueue queuedAudio;
static std::map<std::string,std::string> defaults;
static std::string systemDir,saveDir;static unsigned videos=0,audios=0;
static void logger(retro_log_level,const char* f,...){va_list a;va_start(a,f);vfprintf(stderr,f,a);va_end(a);}
static bool env(unsigned c,void* p){switch(c){
case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:static_cast<retro_log_callback*>(p)->log=logger;return true;
case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(p)=2;return true;
case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:{auto* d=static_cast<retro_core_options_v2*>(p)->definitions;for(;d->key;d++)if(d->default_value)defaults[d->key]=d->default_value;return true;}
case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:{auto* d=static_cast<retro_core_options_v2_intl*>(p)->us->definitions;for(;d->key;d++)if(d->default_value)defaults[d->key]=d->default_value;return true;}
case RETRO_ENVIRONMENT_GET_VARIABLE:{auto* v=static_cast<retro_variable*>(p);if(!strcmp(v->key,"melonds_boot_directly")){v->value="enabled";return true;}if(!strcmp(v->key,"melonds_console_mode")){v->value="DS";return true;}if(!strcmp(v->key,"melonds_threaded_renderer")||!strcmp(v->key,"melonds_opengl_renderer")){v->value="disabled";return true;}auto i=defaults.find(v->key);v->value=i==defaults.end()?nullptr:i->second.c_str();return v->value;}
case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*static_cast<bool*>(p)=false;return true;
case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:*static_cast<const char**>(p)=systemDir.c_str();return true;
case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*static_cast<const char**>(p)=saveDir.c_str();return true;
case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:return *static_cast<unsigned*>(p)==RETRO_PIXEL_FORMAT_XRGB8888;
default:return false;}}
static void video(const void* d,unsigned w,unsigned h,size_t){assert(d&&w==256&&h==384);videos++;}
static size_t audio(const int16_t* data,size_t n){audios+=unsigned(n);assert(queuedAudio.push(data,n));return n;}
int main(int argc,char**){
SDL_SetMainReady();SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_AUDIO)==0);SDL_AudioSpec spec{};spec.freq=32768;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=512;auto device=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);assert(device);queuedAudio.configure(device,32768);
setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);
std::string fixture=argc>1?"build/ds-smoke/invalid/":"build/ds-smoke/normal/";systemDir=std::filesystem::absolute(fixture+"system").string();saveDir=std::filesystem::absolute(fixture+"saves").string();std::filesystem::create_directories(systemDir);std::filesystem::create_directories(saveDir);
auto dll=LoadLibraryA("build/ds-host-source/melonds_libretro.dll");if(!dll){fprintf(stderr,"DLL error %lu\n",GetLastError());return 1;}
#define FN(name) auto name=reinterpret_cast<decltype(&::name)>(GetProcAddress(dll,#name));assert(name)
FN(retro_set_environment);FN(retro_set_video_refresh);FN(retro_set_audio_sample_batch);FN(retro_set_audio_sample);FN(retro_set_input_poll);FN(retro_set_input_state);FN(retro_init);FN(retro_load_game);FN(retro_run);FN(retro_unload_game);FN(retro_deinit);FN(retro_serialize_size);FN(retro_serialize);FN(retro_unserialize);
retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample_batch(audio);retro_set_audio_sample([](int16_t,int16_t){});retro_set_input_poll([]{});retro_set_input_state([](unsigned,unsigned,unsigned,unsigned)->int16_t{return 0;});
puts("core init");retro_init();
// A homebrew DS cartridge created entirely here. ARM9 and ARM7 execute infinite
// branches with IRQs disabled. No Nintendo logo, firmware or commercial code.
std::vector<unsigned char> rom(0x10000);memcpy(rom.data(),"AI1 SMOKE",9);memcpy(rom.data()+0xc,"AI1E",4);
auto word=[&](size_t at,uint32_t n){memcpy(rom.data()+at,&n,4);};
word(0x20,0x200);word(0x24,0x02000000);word(0x28,0x02000000);word(0x2c,4);
word(0x30,0x204);word(0x34,0x03800000);word(0x38,0x03800000);word(0x3c,4);
word(0x80,unsigned(rom.size()));word(0x84,0x200);word(0x200,0xeafffffe);word(0x204,0xeafffffe);
retro_game_info info{"AI1-Smoke.nds",rom.data(),rom.size(),nullptr};puts("load cartridge");
if(argc>1){std::ofstream(systemDir+"/bios9.bin")<<"invalid test bytes";assert(!retro_load_game(&info));retro_deinit();FreeLibrary(dll);puts("PASS: invalid BIOS rejected instead of claiming game loaded");return 0;}
assert(retro_load_game(&info));
for(int i=0;i<12;i++){printf("frame %d\n",i);retro_run();}
assert(videos==12&&audios>0);size_t n=retro_serialize_size();assert(n);std::vector<char> state(n);assert(retro_serialize(state.data(),n));assert(retro_unserialize(state.data(),n));queuedAudio.reset();assert(!queuedAudio.playing&&SDL_GetQueuedAudioSize(device)==0);
retro_unload_game();retro_deinit();SDL_CloseAudioDevice(device);SDL_Quit();FreeLibrary(dll);puts("PASS: actual DS core routes audio through the new SDL queue, video, states/reset and unload (host only)");
}
