// SPDX-License-Identifier: MIT
#include "online_cheats.h"
#include "safe_file.h"
#include "app_version.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#ifdef __SWITCH__
#include <switch.h>
#include <curl/curl.h>
#include "network.h"
#endif
namespace {
const char* origin="https://ai1emu-cheats.freegamez.workers.dev";
bool hex(const std::string& s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string encode(const std::string& s){static const char* h="0123456789ABCDEF";std::string out;for(unsigned char c:s){if(std::isalnum(c)||c=='-'||c=='_'||c=='.')out+=char(c);else{out+='%';out+=h[c>>4];out+=h[c&15];}}return out;}
bool matchesHash(const std::string& data,const std::string& sha){
#ifdef __SWITCH__
 unsigned char bytes[32];sha256CalculateHash(bytes,data.data(),data.size());char value[65]{};for(int i=0;i<32;++i)std::snprintf(value+i*2,3,"%02x",bytes[i]);return sha==value;
#else
 (void)data;(void)sha;return false; // Preview cannot download/import cheats; tests exercise the real parsers.
#endif
}
}
bool parseCheatCatalog(const std::string& text,std::vector<CheatCandidate>& out){
 out.clear();if(text.size()>32768)return false;std::istringstream stream(text);std::string line;if(!std::getline(stream,line)||line!="AI1CHEATS1")return false;
 while(std::getline(stream,line)){if(line.empty())continue;if(out.size()>=24)return false;std::vector<std::string> parts;std::istringstream row(line);std::string part;while(std::getline(row,part,'\t'))parts.push_back(part);if(parts.size()!=5)return false;
  CheatCandidate e;e.id=parts[0];e.sha=parts[2];e.title=parts[4];if(!hex(e.id,24)||!hex(e.sha,64)||e.title.empty()||e.title.size()>240||e.title.find_first_of("\r\n")!=std::string::npos)return false;
  if(parts[1].empty()||parts[1].find_first_not_of("0123456789")!=std::string::npos||parts[3].empty()||parts[3].find_first_not_of("0123456789")!=std::string::npos)return false;
  try{size_t n=0;e.bytes=std::stoul(parts[1],&n);if(n!=parts[1].size()||e.bytes==0||e.bytes>1024*1024)return false;auto count=std::stoul(parts[3],&n);if(n!=parts[3].size()||count==0||count>512)return false;e.count=unsigned(count);}catch(...){return false;}out.push_back(e);
 }return true;
}
OnlineCheats::~OnlineCheats(){cancelled=true;if(thread)SDL_WaitThread(thread,nullptr);}
void OnlineCheats::set(Phase next,const std::string& text){{std::lock_guard<std::mutex> lock(mutex);detail=text;}state=next;}
std::string OnlineCheats::message(){std::lock_guard<std::mutex> lock(mutex);return detail;}
std::string OnlineCheats::cachedFile(){std::lock_guard<std::mutex> lock(mutex);return cachePath;}
std::vector<CheatCandidate> OnlineCheats::candidates(){std::lock_guard<std::mutex> lock(mutex);return matches;}
void OnlineCheats::start(){if(thread){SDL_WaitThread(thread,nullptr);thread=nullptr;}cancelled=false;thread=SDL_CreateThread(entry,"AI1Cheats",this);if(!thread)set(Failed,"Could not start download");}
void OnlineCheats::search(const std::string& platform,const std::string& title){if(phase()==Searching||phase()==Downloading)return;downloading=false;requestPath="/v1/search?format=tsv&platform="+encode(platform)+"&title="+encode(title);set(Searching,"Checking online cheat database...");start();}
void OnlineCheats::download(const CheatCandidate& e,const std::string& root){if(phase()==Searching||phase()==Downloading)return;downloading=true;selected=e;requestPath="/cheats/files/"+e.id+".cht";{std::lock_guard<std::mutex> lock(mutex);cachePath=root+e.id+".cht";}set(Downloading,"Downloading cheats...");start();}
int OnlineCheats::entry(void* p){auto* self=static_cast<OnlineCheats*>(p);try{self->run();}catch(...){self->set(Failed,"Cheat download failed; existing cheats unchanged");}return 0;}
bool OnlineCheats::fetch(std::string& output,size_t limit){
#ifdef __SWITCH__
 struct Sink{std::string data;size_t max;};Sink sink{{},limit};CURL* curl=curl_easy_init();if(!curl){set(Failed,"Could not create HTTPS request");return false;}
 const auto url=std::string(origin)+requestPath;char error[CURL_ERROR_SIZE]{};
 curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,error);curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,25L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_USERAGENT,"AI1Emu/" AI1EMU_VERSION);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,+[](char* data,size_t a,size_t b,void* p)->size_t{auto& s=*static_cast<Sink*>(p);if(a&&b>SIZE_MAX/a)return 0;size_t n=a*b;if(n>s.max-s.data.size())return 0;s.data.append(data,n);return n;});curl_easy_setopt(curl,CURLOPT_WRITEDATA,&sink);
 curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,+[](void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{return static_cast<OnlineCheats*>(p)->cancelled?1:0;});curl_easy_setopt(curl,CURLOPT_XFERINFODATA,this);
 auto result=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(curl);
 if(result!=CURLE_OK||status!=200){set(Failed,cancelled?"Download cancelled":result==CURLE_PEER_FAILED_VERIFICATION?"HTTPS certificate error. Check Switch date and time.":"Cheat service unavailable. Check Wi-Fi and retry.");return false;}output=std::move(sink.data);return true;
#else
 (void)output;(void)limit;set(Failed,"Online downloads require the Switch build");return false;
#endif
}
void OnlineCheats::run(){
 if(downloading){std::ifstream f(cachePath,std::ios::binary|std::ios::ate);if(f&&f.tellg()==std::streamoff(selected.bytes)){std::string data(selected.bytes,'\0');f.seekg(0);f.read(data.data(),data.size());if(f&&matchesHash(data,selected.sha)){set(Ready,"Using cached cheats");return;}}}
#ifdef __SWITCH__
 if(!networkStart()){set(Failed,"Network unavailable. Connect Wi-Fi and retry.");return;}
#endif
 std::string data;if(!fetch(data,downloading?selected.bytes:32768))return;if(cancelled){set(Failed,"Download cancelled");return;}
 if(!downloading){std::vector<CheatCandidate> next;if(!parseCheatCatalog(data,next)){set(Failed,"Invalid cheat catalog response");return;}{std::lock_guard<std::mutex> lock(mutex);matches=std::move(next);}set(Choices,"Choose the correct game region and revision");return;}
 if(data.size()!=selected.bytes||!matchesHash(data,selected.sha)){set(Failed,"Cheat verification failed; existing cheats unchanged");return;}
 if(!safeWriteFile(cachePath,data.data(),data.size())){set(Failed,"Could not cache cheats. Check free SD space.");return;}set(Ready,"Cheats downloaded; disabled until you enable them");
}
