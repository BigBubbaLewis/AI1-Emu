#include "online_updater.h"
#include "app_version.h"
#include "update_install.h"
#include "network.h"
#include <switch.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cerrno>
#include <filesystem>
namespace {
constexpr const char* coresDir="sdmc:/switch/ai1emu/cores/";
struct Sink { FILE* file=nullptr;std::string data;size_t bytes=0,limit=4096;Sha256Context hash{}; };
size_t receive(char* data,size_t size,size_t count,void* context){
 auto& sink=*static_cast<Sink*>(context);size_t n=size*count;if(n>sink.limit-sink.bytes)return 0;
 if(sink.file){if(fwrite(data,1,n,sink.file)!=n)return 0;sha256ContextUpdate(&sink.hash,data,n);}else sink.data.append(data,n);sink.bytes+=n;return n;
}
bool nroHeader(const char* staged){FILE* f=fopen(staged,"rb");if(!f)return false;unsigned char header[32]{};bool ok=fread(header,1,sizeof(header),f)==sizeof(header)&&memcmp(header+16,"NRO0",4)==0;fclose(f);return ok;}
// SHA-256 of a local file when it has the expected size; empty otherwise. Lets the
// emulator update skip files that are already identical on the SD card.
std::string localSha(const std::string& path,size_t size){
 std::error_code e;if(std::filesystem::file_size(path,e)!=size||e)return {};
 FILE* f=fopen(path.c_str(),"rb");if(!f)return {};
 Sha256Context ctx;sha256ContextCreate(&ctx);std::vector<unsigned char> buffer(1024*1024);size_t n;
 while((n=fread(buffer.data(),1,buffer.size(),f))>0)sha256ContextUpdate(&ctx,buffer.data(),n);
 bool ok=!ferror(f);fclose(f);if(!ok)return {};
 unsigned char digest[32];sha256ContextGetHash(&ctx,digest);char hex[65]{};for(int i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",digest[i]);return hex;
}
}
OnlineUpdater::~OnlineUpdater(){cancel=true;if(thread)SDL_WaitThread(thread,nullptr);}
void OnlineUpdater::set(Phase next,const std::string& text){std::lock_guard<std::mutex> lock(mutex);detail=text;state=next;}
std::string OnlineUpdater::message(){std::lock_guard<std::mutex> lock(mutex);return detail;}
bool OnlineUpdater::testChannel(){std::ifstream f("sdmc:/switch/ai1emu/update-channel.txt");std::string word;f>>word;for(char& c:word)c=char(std::tolower((unsigned char)c));return word=="test";}
std::string OnlineUpdater::installedCores(){std::ifstream f(std::string(coresDir)+"cores.version");std::string v;f>>v;return v;}
std::string OnlineUpdater::label(){
 std::string suffix=test?" (test channel)":"";
 switch(phase()){
 case Checking:return "Checking..."+suffix;
 case Available:return (coresMode?"Download emulator update":ds?"Download DS audio update":n64?"Download N64 support":"Download update")+suffix;
 case Downloading:return "Downloading "+std::to_string(percent.load())+"%";
 case Ready:return "Install and close app";
 case Current:return "Up to date - check again"+suffix;
 case Failed:return "Retry update check"+suffix;
 default:return (n64?"Install N64 support":"Check for updates")+suffix;
 }
}
void OnlineUpdater::check(){
 if(phase()==Checking||phase()==Downloading)return;
 test=testChannel();origin=test?AI1EMU_TEST_ORIGIN:AI1EMU_UPDATE_ORIGIN;coresMode=false;
 // The test channel delivers every emulator through cores.txt instead of the
 // older one-off N64 / DS component channels.
 FILE* f=fopen("sdmc:/switch/ai1emu/cores/mupen64plus-next.nro","rb");n64=!test&&!f;if(f)fclose(f);
 std::string revision;{std::ifstream marker("sdmc:/switch/ai1emu/cores/melonds-audio.version");marker>>revision;}
 std::ifstream dsCore("sdmc:/switch/ai1emu/cores/melonds.nro",std::ios::binary);ds=!test&&!n64&&(revision!="1"||!dsCore);
 target=ds?"sdmc:/switch/ai1emu/cores/melonds.nro":n64?"sdmc:/switch/ai1emu/cores/mupen64plus-next.nro":"sdmc:/switch/ai1emu/AI1Emu.nro";staged=target+".update";backup=target+".bak";start(false);
}
void OnlineUpdater::download(){if(phase()==Available)start(true);}
void OnlineUpdater::start(bool file){if(thread){SDL_WaitThread(thread,nullptr);thread=nullptr;}fetchingFile=file;cancel=false;percent=0;set(file?Downloading:Checking,file?"Downloading from Cloudflare":"Connecting to update service");thread=SDL_CreateThread(entry,"AI1Update",this);if(!thread)set(Failed,"Could not start update check");}
int OnlineUpdater::entry(void* data){static_cast<OnlineUpdater*>(data)->run();return 0;}
int OnlineUpdater::progress(void* data,curl_off_t total,curl_off_t downloaded,curl_off_t,curl_off_t){auto& self=*static_cast<OnlineUpdater*>(data);if(total>0&&!self.coresMode)self.percent=unsigned(std::min<curl_off_t>(100,downloaded*100/total));return self.cancel?1:0;}

// Downloads origin+path into `text` (small files) or `file` (verified binaries).
bool OnlineUpdater::fetch(const std::string& path,std::string* text,const char* file,size_t limit,std::string* sha,long timeout){
 fetchFailure.clear(); CURL* curl=curl_easy_init();if(!curl){fetchFailure="Could not create HTTPS request";return false;}
 Sink sink;sink.limit=limit;
 if(file){sink.file=fopen(file,"wb");if(!sink.file){fetchFailure="Cannot write update file: "+std::string(std::strerror(errno));curl_easy_cleanup(curl);return false;}sha256ContextCreate(&sink.hash);}
 std::string url=origin+path;
 char error[CURL_ERROR_SIZE]{}; curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,error);
 curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,timeout);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_USERAGENT,"AI1Emu/" AI1EMU_VERSION);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&sink);curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,this);
 CURLcode result=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(curl);
 bool flushed=true;int fileError=0;if(sink.file){flushed=fflush(sink.file)==0;if(!flushed)fileError=errno;if(flushed&&fsync(fileno(sink.file))!=0){flushed=false;fileError=errno;}if(fclose(sink.file)!=0){flushed=false;if(!fileError)fileError=errno;}}
 if(result!=CURLE_OK||status!=200||!flushed){if(result!=CURLE_OK)fetchFailure="HTTPS error "+std::to_string(int(result))+": "+(error[0]?std::string(error):std::string(curl_easy_strerror(result)));else if(status!=200)fetchFailure="Update server returned HTTP "+std::to_string(status);else fetchFailure="SD flush failed: "+std::string(std::strerror(fileError));if(file)std::remove(file);return false;}
 if(text)*text=std::move(sink.data);
 if(sha){unsigned char digest[32];sha256ContextGetHash(&sink.hash,digest);char hex[65]{};for(int i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",digest[i]);*sha=hex;}
 if(file&&sink.bytes!=limit){fetchFailure="Incomplete update: "+std::to_string(sink.bytes)+" of "+std::to_string(limit)+" bytes";std::remove(file);return false;}
 return true;
}

void OnlineUpdater::run(){
 if(!networkStart()){set(Failed,"Network unavailable. Connect Wi-Fi and retry.");return;}
 if(coresMode){runCores();return;}
 std::string channel=test?"Test channel. ":"";
 if(!fetchingFile){
  std::string text;
  if(!fetch(ds?"/ds.json":n64?"/n64.json":"/latest.json",&text,nullptr,4096,nullptr,20)){set(Failed,channel+fetchFailure+"; app unchanged.");return;}
  UpdateManifest next;if(!parseUpdateManifest(text,next,n64,ds)){set(Failed,"Invalid update information; app unchanged.");return;}
  manifest=next;
  if(n64||ds||newerVersion(manifest.version,AI1EMU_VERSION)){set(Available,channel+(ds?"DS audio update is available. Press A or tap to download.":n64?"N64 support is available. Press A or tap to download.":"Version "+manifest.version+" is available. Press A or tap to download."));return;}
  // App is current: are the emulator files current too?
  std::string list;CoreList cores;
  if(fetch("/cores.txt",&list,nullptr,4096,nullptr,20)&&parseCoreList(list,cores)&&cores.version!=installedCores()&&!newerVersion(cores.version,AI1EMU_VERSION)){
   coreList=cores;coresMode=true;
   size_t total=0;for(auto& c:cores.cores)total+=c.size;
   set(Available,channel+"Emulator update "+cores.version+" is available ("+std::to_string(cores.cores.size())+" files, "+std::to_string(total/(1024*1024))+" MB). Press A or tap to download. No restart needed.");
   return;
  }
  std::string emulators=installedCores();
  set(Current,channel+"App "+std::string(AI1EMU_VERSION)+(emulators.empty()?"":" and emulators "+emulators)+" are up to date. Games stay offline.");return;
 }
 std::string sha;
 if(!fetch(manifest.path,nullptr,staged.c_str(),manifest.size,&sha,300)){set(Failed,channel+fetchFailure+"; app unchanged.");return;}
 if(manifest.sha256!=sha||!nroHeader(staged.c_str())){std::remove(staged.c_str());set(Failed,"Update verification failed; app unchanged.");return;}
 set(Ready,"Verified update. Press A or tap to install and close.");
}

// Downloads every emulator file, verifies all of them, then swaps them in.
// Nothing is replaced unless every file verified.
void OnlineUpdater::runCores(){
 std::error_code directoryError; std::filesystem::create_directories(coresDir,directoryError); if(directoryError){set(Failed,"Cannot create emulator update folder: "+directoryError.message());return;}
 // Files already identical on the SD card (same size and SHA-256) are not downloaded again.
 std::vector<bool> needed;size_t total=0,done=0,skipped=0;
 for(auto& c:coreList.cores){
  set(Downloading,"Checking "+c.name+" on the SD card");
  bool same=localSha(std::string(coresDir)+c.name,c.size)==c.sha256;
  needed.push_back(!same);if(same)++skipped;else total+=c.size;
 }
 std::vector<std::string> stagedFiles;
 auto cleanup=[&]{for(auto& s:stagedFiles)std::remove(s.c_str());};
 for(size_t index=0;index<coreList.cores.size();++index){
  auto& c=coreList.cores[index];
  if(!needed[index])continue;
  if(cancel){cleanup();set(Failed,"Emulator update cancelled; emulators unchanged.");return;}
  std::string file=std::string(coresDir)+c.name+".update",sha;
  set(Downloading,"Downloading "+c.name+" ("+std::to_string(done/(1024*1024))+" of "+std::to_string(total/(1024*1024))+" MB)");
  percent=unsigned(total?done*100/total:0);
  if(!fetch(c.path,nullptr,file.c_str(),c.size,&sha,600)){cleanup();set(Failed,c.name+": "+fetchFailure+"; emulators unchanged.");return;}
  stagedFiles.push_back(file);
  if(sha!=c.sha256||!nroHeader(file.c_str())){cleanup();set(Failed,c.name+" failed verification; emulators unchanged.");return;}
  done+=c.size;
 }
 for(size_t index=0;index<coreList.cores.size();++index){
  auto& c=coreList.cores[index];
  if(!needed[index])continue;
  std::string dest=std::string(coresDir)+c.name;
  if(!installNroUpdate(dest.c_str(),(dest+".update").c_str(),(dest+".bak").c_str(),std::rename,true)){cleanup();set(Failed,"Could not replace "+c.name+". Check the SD card; earlier files may be updated.");return;}
 }
 {std::ofstream marker(std::string(coresDir)+"cores.version");marker<<coreList.version<<"\n";}
 {std::ofstream marker(std::string(coresDir)+"melonds-audio.version");marker<<"1\n";}
 coresMode=false;percent=100;
 size_t updated=coreList.cores.size()-skipped;
 set(Current,std::string(test?"Test channel. ":"")+"Emulators updated to "+coreList.version+" ("+std::to_string(updated)+(updated==1?" file":" files")+" downloaded, "+std::to_string(skipped)+" already current). Ready to play.");
}

bool OnlineUpdater::apply(){
 if(phase()!=Ready)return false;if(thread){SDL_WaitThread(thread,nullptr);thread=nullptr;}
 bool installed=installNroUpdate(target.c_str(),staged.c_str(),backup.c_str(),std::rename,n64||ds);if(installed&&ds){std::ofstream marker("sdmc:/switch/ai1emu/cores/melonds-audio.version");marker<<"1\n";marker.close();if(!marker)return false;}return installed;
}
