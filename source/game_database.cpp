#include "game_database.h"
#include "runtime_paths.h"
#include "platform_library.h"
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <cctype>
struct GameDB {bool loaded=false;std::map<std::string,std::string> exact;std::map<std::string,std::vector<std::string>> serials,names;};
static std::string normalized(std::string s){std::string out;int depth=0;for(size_t i=0;i<s.size();++i){unsigned char c=s[i];if(c=='('||c=='['){++depth;continue;}if(c==')'||c==']'){if(depth)--depth;continue;}if(depth)continue;if(c==0xc3&&i+1<s.size()&&(unsigned char)s[i+1]==0xa9){out+='e';++i;}else if(c<128&&std::isalnum(c))out+=char(std::tolower(c));}return out;}
static std::map<Platform,GameDB>& databases(){static std::map<Platform,GameDB> all;return all;}
static void loadDatabase(GameDB& db,const std::string& slug){if(!db.loaded){db.loaded=true;std::ifstream input(retroverseAssets()+slug+"-database.tsv");std::string line;while(std::getline(input,line)){size_t a=line.find('\t'),b=line.find('\t',a+1),c=line.find('\t',b+1);if(a==std::string::npos||b==std::string::npos||c==std::string::npos)continue;std::string serial=line.substr(b+1,c-b-1),title=line.substr(c+1);if(!title.empty()&&title.back()=='\r')title.pop_back();db.exact[title]=title;if(!serial.empty())db.serials[serial].push_back(title);db.names[normalized(title)].push_back(title);}}}
std::string gameTitle(const std::string& path){
 static std::map<std::string,std::string> resolved;
 auto known=resolved.find(path);if(known!=resolved.end())return known->second;
 auto platform=detectPlatform(path);auto& db=databases()[platform];std::string slug=platformInfo(platform).slug;
 loadDatabase(db,slug);
 std::string name=std::filesystem::path(path).stem().string(),title;
 std::vector<std::string> metadata={retroverseRoot()+"metadata/"+slug+"/"+name+".txt"};if(platform==Platform::GB||platform==Platform::GBC)metadata.push_back(retroverseRoot()+"metadata/"+(platform==Platform::GB?"gbc":"gb")+"/"+name+".txt");if(platform==Platform::GBA)metadata.push_back(retroverseRoot()+"metadata/"+name+".txt");
 for(auto& file:metadata){std::ifstream meta(file);std::string line;while(std::getline(meta,line))if(line.rfind("title=",0)==0){title=line.substr(6);if(!title.empty()&&title.back()=='\r')title.pop_back();break;}if(!title.empty())break;}
 if(title.empty()&&db.exact.count(name))title=db.exact[name];
 // The header's game code identifies the original release, but ROM hacks keep it too
 // ("Pokemon Unbound" carries FireRed's BPRE). Use it only when the file name looks like
 // that release, e.g. "Pokemon Emerald (U)" for "Pokemon - Emerald Version (USA, Europe)".
 if(title.empty()&&(platform==Platform::GBA||isDualScreen(platform))){std::ifstream rom(path,std::ios::binary);char serial[4]{};rom.seekg(platform==Platform::GBA?0xac:0x0c);rom.read(serial,4);auto found=db.serials.find(std::string(serial,4));if(rom&&found!=db.serials.end()){std::string pick=found->second.front();for(auto& candidate:found->second)if(candidate.find("(Rev ")==std::string::npos){pick=candidate;break;}const std::string mine=normalized(name);if(!mine.empty()&&normalized(pick).find(mine)!=std::string::npos)title=pick;}}
 if(title.empty()){auto found=db.names.find(normalized(name));if(found!=db.names.end()){title=found->second.front();for(auto& candidate:found->second)if(candidate.find("(USA")!=std::string::npos&&candidate.find("(Rev ")==std::string::npos){title=candidate;break;}}}
 if(title.empty()&&(platform==Platform::GB||platform==Platform::GBC)){auto other=platform==Platform::GB?Platform::GBC:Platform::GB;auto& fallback=databases()[other];loadDatabase(fallback,platformInfo(other).slug);auto exact=fallback.exact.find(name);if(exact!=fallback.exact.end())title=exact->second;else{auto candidates=fallback.names.find(normalized(name));if(candidates!=fallback.names.end()&&!candidates->second.empty()){title=candidates->second.front();for(auto& candidate:candidates->second)if(candidate.find("(USA")!=std::string::npos&&candidate.find("(Rev ")==std::string::npos){title=candidate;break;}}}}
 return resolved[path]=title.empty()?name:title;
}
// True when the resolved title is a release in the bundled No-Intro table
// (or the sibling GB/GBC table), rather than a filename fallback.
bool gameRecognized(const std::string& path){
 auto title=gameTitle(path);auto platform=detectPlatform(path);
 auto& db=databases()[platform];loadDatabase(db,platformInfo(platform).slug);if(db.exact.count(title))return true;
 if(platform==Platform::GB||platform==Platform::GBC){auto other=platform==Platform::GB?Platform::GBC:Platform::GB;auto& fallback=databases()[other];loadDatabase(fallback,platformInfo(other).slug);return fallback.exact.count(title)>0;}
 return false;
}
