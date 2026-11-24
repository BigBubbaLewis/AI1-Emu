#include "online_updater.h"
#include "app_version.h"
#include "update_install.h"
#include <switch.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <fstream>
namespace {
struct Sink { FILE* file=nullptr;std::string data;size_t bytes=0,limit=4096;Sha256Context hash{}; };
size_t receive(char* data,size_t size,size_t count,void* context){
 auto& sink=*static_cast<Sink*>(context);size_t n=size*count;if(n>sink.limit-sink.bytes)return 0;
 if(sink.file){if(fwrite(data,1,n,sink.file)!=n)return 0;sha256ContextUpdate(&sink.hash,data,n);}else sink.data.append(data,n);sink.bytes+=n;return n;
}
bool nroHeader(const char* staged){FILE* f=fopen(staged,"rb");if(!f)return false;unsigned char header[32]{};bool ok=fread(header,1,sizeof(header),f)==sizeof(header)&&memcmp(header+16,"NRO0",4)==0;fclose(f);return ok;}
}
OnlineUpdater::~OnlineUpdater(){cancel=true;if(thread)SDL_WaitThread(thread,nullptr);if(sockets){curl_global_cleanup();socketExit();}}
void OnlineUpdater::set(Phase next,const std::string& text){std::lock_guard<std::mutex> lock(mutex);detail=text;state=next;}
std::string OnlineUpdater::message(){std::lock_guard<std::mutex> lock(mutex);return detail;}
std::string OnlineUpdater::label(){switch(phase()){case Checking:return "Checking...";case Available:return ds?"Download DS audio update":n64?"Download N64 support":"Download update";case Downloading:return "Downloading "+std::to_string(percent.load())+"%";case Ready:return "Install and close app";case Current:return "Up to date - check again";case Failed:return "Retry update check";default:return n64?"Install N64 support":"Check for updates";}}
void OnlineUpdater::check(){if(phase()==Checking||phase()==Downloading)return;FILE* f=fopen("sdmc:/switch/ai1emu/cores/mupen64plus-next.nro","rb");n64=!f;if(f)fclose(f);std::string revision;{std::ifstream marker("sdmc:/switch/ai1emu/cores/melonds-audio.version");marker>>revision;}std::ifstream dsCore("sdmc:/switch/ai1emu/cores/melonds.nro",std::ios::binary);ds=!n64&&(revision!="1"||!dsCore);target=ds?"sdmc:/switch/ai1emu/cores/melonds.nro":n64?"sdmc:/switch/ai1emu/cores/mupen64plus-next.nro":"sdmc:/switch/ai1emu/AI1Emu.nro";staged=target+".update";backup=target+".bak";start(false);}
void OnlineUpdater::download(){if(phase()==Available)start(true);}
void OnlineUpdater::start(bool file){if(thread){SDL_WaitThread(thread,nullptr);thread=nullptr;}fetchingFile=file;cancel=false;percent=0;set(file?Downloading:Checking,file?"Downloading from Cloudflare":"Connecting to update service");thread=SDL_CreateThread(entry,"AI1Update",this);if(!thread)set(Failed,"Could not start update check");}
int OnlineUpdater::entry(void* data){static_cast<OnlineUpdater*>(data)->run();return 0;}
int OnlineUpdater::progress(void* data,curl_off_t total,curl_off_t downloaded,curl_off_t,curl_off_t){auto& self=*static_cast<OnlineUpdater*>(data);if(total>0)self.percent=unsigned(std::min<curl_off_t>(100,downloaded*100/total));return self.cancel?1:0;}
void OnlineUpdater::run(){
 if(!sockets){if(R_FAILED(socketInitializeDefault())){set(Failed,"Network unavailable. Connect Wi-Fi and retry.");return;}sockets=true;if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){socketExit();sockets=false;set(Failed,"Could not initialize HTTPS");return;}}
 CURL* curl=curl_easy_init();if(!curl){set(Failed,"Could not start HTTPS");return;}
 Sink sink;std::string url=AI1EMU_UPDATE_ORIGIN;
 if(fetchingFile){url+=manifest.path;sink.limit=manifest.size;sink.file=fopen(staged.c_str(),"wb");if(!sink.file){curl_easy_cleanup(curl);set(Failed,"Cannot write update. Check SD card and app path.");return;}sha256ContextCreate(&sink.hash);}else url+=ds?"/ds.json":n64?"/n64.json":"/latest.json";
 curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,fetchingFile?300L:20L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_USERAGENT,"AI1Emu/" AI1EMU_VERSION);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&sink);curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,this);
 CURLcode result=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(curl);
 bool flushed=true;if(sink.file){flushed=fflush(sink.file)==0;if(flushed)flushed=fsync(fileno(sink.file))==0;if(fclose(sink.file)!=0)flushed=false;}
 if(result!=CURLE_OK||status!=200||!flushed){if(fetchingFile)std::remove(staged.c_str());set(Failed,"Update unavailable. Check Wi-Fi and retry; app unchanged.");return;}
 if(!fetchingFile){UpdateManifest next;if(!parseUpdateManifest(sink.data,next,n64,ds)){set(Failed,"Invalid update information; app unchanged.");return;}manifest=next;if(n64||ds||newerVersion(manifest.version,AI1EMU_VERSION))set(Available,ds?"DS audio update is available. Press ZR or tap to download.":n64?"N64 support is available. Press ZR or tap to download.":"Version "+manifest.version+" is available. Press ZR or tap to download.");else set(Current,"Version " AI1EMU_VERSION " is current. Games stay offline.");return;}
 unsigned char digest[32];sha256ContextGetHash(&sink.hash,digest);char hex[65]{};for(int i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",digest[i]);
 if(sink.bytes!=manifest.size||manifest.sha256!=hex||!nroHeader(staged.c_str())){std::remove(staged.c_str());set(Failed,"Update verification failed; app unchanged.");return;}
 set(Ready,"Verified update. Tap again or press ZR to install and close.");
}
bool OnlineUpdater::apply(){
 if(phase()!=Ready)return false;if(thread){SDL_WaitThread(thread,nullptr);thread=nullptr;}
 bool installed=installNroUpdate(target.c_str(),staged.c_str(),backup.c_str(),std::rename,n64||ds);if(installed&&ds){std::ofstream marker("sdmc:/switch/ai1emu/cores/melonds-audio.version");marker<<"1\n";marker.close();if(!marker)return false;}return installed;
}
