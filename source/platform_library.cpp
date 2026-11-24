// SPDX-License-Identifier: MIT
#include "platform_library.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <map>
#include <array>
#include <tuple>
namespace fs=std::filesystem;
const std::vector<PlatformInfo>& platforms(){static const std::vector<PlatformInfo> entries={
 {Platform::GBA,"gba","Game Boy Advance","GBA",""},{Platform::GB,"gb","Game Boy","GB",""},{Platform::GBC,"gbc","Game Boy Color","GBC",""},
 {Platform::NES,"nes","Nintendo Entertainment System","NES","fceumm.nro"},{Platform::SNES,"snes","Super Nintendo","SNES","snes9x.nro"},
 {Platform::Genesis,"genesis","Sega Genesis / Mega Drive","Genesis","genesis-plus-gx.nro"},{Platform::MasterSystem,"sms","Sega Master System","SMS","genesis-plus-gx.nro"},{Platform::GameGear,"gg","Sega Game Gear","GG","genesis-plus-gx.nro"},
 {Platform::DS,"nds","Nintendo DS","DS","melonds.nro"},{Platform::DSi,"dsi","Nintendo DSi (experimental)","DSi","melonds.nro"},{Platform::N64,"n64","Nintendo 64 (experimental)","N64","mupen64plus-next.nro"}};return entries;}
const PlatformInfo& platformInfo(Platform p){for(auto& info:platforms())if(info.id==p)return info;static const PlatformInfo unknown{Platform::Unknown,"unknown","Unknown platform","?",""};return unknown;}
static std::string lower(std::string text){std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});return text;}
static Platform detectUncached(const std::string& path){
 auto ext=lower(fs::path(path).extension().string());auto folder=lower(fs::path(path).parent_path().filename().string());
 if(ext==".gba")return Platform::GBA;if(ext==".gbc")return Platform::GBC;
 if(ext==".gb"){std::ifstream f(path,std::ios::binary);unsigned char flag=0;f.seekg(0x143);f.read((char*)&flag,1);return f&&(flag&0x80)?Platform::GBC:Platform::GB;}
 if(ext==".nes")return Platform::NES;if(ext==".sfc"||ext==".smc")return Platform::SNES;
 if(ext==".md"||ext==".gen"||ext==".smd")return Platform::Genesis;
 if(ext==".sms")return Platform::MasterSystem;if(ext==".gg")return Platform::GameGear;
 if(ext==".bin"){std::ifstream f(path,std::ios::binary);char magic[4]{};f.seekg(0x100);f.read(magic,4);if(folder=="genesis"||folder=="megadrive"||std::string(magic,4)=="SEGA")return Platform::Genesis;}
 if(ext==".z64"||ext==".v64"||ext==".n64")return Platform::N64;
 if(ext==".dsi")return Platform::DSi;
 if(ext==".nds"){if(folder=="dsi")return Platform::DSi;std::ifstream f(path,std::ios::binary);unsigned char unit=0;f.seekg(0x12);f.read((char*)&unit,1);return f&&unit==3?Platform::DSi:Platform::DS;}
 return Platform::Unknown;
}
Platform detectPlatform(const std::string& path){static std::map<std::string,Platform> cache;auto found=cache.find(path);if(found!=cache.end())return found->second;return cache[path]=detectUncached(path);}
// Only byte-identical files on the same console are collapsed in the library.
// Files and all save histories remain on disk; revisions and hacks stay distinct.
static bool sameRom(const std::string& a,const std::string& b){
 std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);if(!x||!y)return false;
 std::array<char,65536> left{},right{};
 do{x.read(left.data(),left.size());y.read(right.data(),right.size());if(x.gcount()!=y.gcount()||!std::equal(left.begin(),left.begin()+x.gcount(),right.begin()))return false;}while(x.gcount());
 return x.eof()&&y.eof();
}
static bool romSample(const std::string& path,uintmax_t size,uint64_t& hash){
 std::ifstream f(path,std::ios::binary);if(!f)return false;std::array<char,4096> bytes{};hash=14695981039346656037ULL;
 for(uintmax_t at:{uintmax_t(0),size/2,size>4096?size-4096:0}){f.clear();f.seekg(at);size_t n=std::min(uintmax_t(bytes.size()),size-at);if(!f.read(bytes.data(),n))return false;for(size_t i=0;i<n;++i){hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ULL;}}return true;
}
static std::tuple<int,int64_t,fs::file_time_type> progressRank(const std::string& root,const std::string& game){
 std::pair<int,fs::file_time_type> rank{0,fs::file_time_type::min()};std::error_code ec;auto prefix=gameSavePrefix(root,game);
 auto consider=[&](const fs::path& file,int tier){if(!fs::is_regular_file(file,ec)||!fs::file_size(file,ec)||ec){ec.clear();return;}auto stamp=fs::last_write_time(file,ec);if(!ec)rank=std::max(rank,std::make_pair(tier,stamp));ec.clear();};
 consider(prefix+".sav",2);consider(prefix+".state",2);consider(prefix+".lastplayed",1);
 fs::recursive_directory_iterator it(prefix+".files",fs::directory_options::skip_permission_denied,ec),end;for(;!ec&&it!=end;it.increment(ec))if(it->is_regular_file(ec))consider(it->path(),2);
 int64_t played=0;{std::ifstream stamp(prefix+".lastplayed");if(!(stamp>>played)||played<0||played>4102444800LL)played=0;}
 return std::make_tuple(rank.first,played,rank.second);
}
std::vector<std::string> scanLibrary(const std::string& root){
 std::vector<std::string> all;std::error_code error;fs::recursive_directory_iterator it(root+"roms",fs::directory_options::skip_permission_denied,error),end;
 for(;!error&&it!=end;it.increment(error))if(it->is_regular_file(error)&&detectPlatform(it->path().string())!=Platform::Unknown)all.push_back(it->path().string());
 std::sort(all.begin(),all.end());all.erase(std::unique(all.begin(),all.end()),all.end());std::map<std::pair<Platform,uintmax_t>,std::vector<std::string>> sizes;
 for(auto& game:all){auto n=fs::file_size(game,error);if(!error&&n)sizes[{detectPlatform(game),n}].push_back(game);error.clear();}
 std::map<std::string,bool> hidden;std::map<std::string,std::string> aliases;
 for(auto& group:sizes){if(group.second.size()<2)continue;std::map<uint64_t,std::vector<std::string>> samples;
  for(auto& game:group.second){uint64_t key;if(romSample(game,group.first.second,key))samples[key].push_back(game);}
  for(auto& sample:samples){std::vector<std::string> representatives;
   for(auto& game:sample.second){bool duplicate=false;for(auto& kept:representatives)if(sameRom(kept,game)){if(progressRank(root,game)>progressRank(root,kept)){for(auto& alias:aliases)if(alias.second==kept)alias.second=game;aliases[kept]=game;hidden[kept]=true;kept=game;}else {hidden[game]=true;aliases[game]=kept;}duplicate=true;break;}if(!duplicate)representatives.push_back(game);}
  }
 }
 if(!aliases.empty()){fs::create_directories(root+"logs",error);if(!error){std::ofstream report(root+"logs/library-duplicates.txt");report<<"Most recent scan with byte-identical copies. Files/saves are preserved.\nHidden copy -> visible copy (most recent existing save/state preferred)\n";for(auto& alias:aliases)report<<alias.first<<" -> "<<alias.second<<"\n";}}
 std::vector<std::string> result;for(auto& game:all)if(!hidden[game])result.push_back(game);return result;
}
std::vector<std::string> filterLibrary(const std::vector<std::string>& all,int category){if(category<=0)return all;std::vector<std::string> out;for(auto& p:all)if(int(detectPlatform(p))==category-1)out.push_back(p);return out;}
std::string gameSavePrefix(const std::string& root,const std::string& game){
 auto p=detectPlatform(game);std::string file=fs::path(game).filename().string(),legacy=root+"saves/"+file;
 if(p==Platform::GBA&&fs::path(game).parent_path().lexically_normal()==fs::path(root+"roms").lexically_normal()){for(const char* suffix:{".sav",".state",".lastplayed"})if(fs::exists(legacy+suffix))return legacy;}
 // Relative path disambiguates identical filenames in different game folders.
 std::error_code error;std::string relative=fs::relative(game,root+"roms",error).generic_string();if(error)relative=game;
 uint64_t hash=14695981039346656037ULL;for(unsigned char c:relative){hash^=c;hash*=1099511628211ULL;}
 std::ostringstream suffix;suffix<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
 return root+"saves/"+platformInfo(p).slug+"/"+file+"."+suffix.str();
}
bool isDualScreen(Platform p){return p==Platform::DS||p==Platform::DSi;}
