#include "game_database.h"
#include "runtime_paths.h"
#include "platform_library.h"
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <cctype>
#include <algorithm>
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
// A patched game (ROM hack) keeps the original release's game code in its header but has a
// file name that is not that release. Returns the original release, or "" when the game is
// not a recognised patch (unknown code, or the name matches the release).
std::string gamePatchedFrom(const std::string& path){
 static std::map<std::string,std::string> resolved;
 auto known=resolved.find(path);if(known!=resolved.end())return known->second;
 auto platform=detectPlatform(path);std::string base;
 if(platform==Platform::GBA||isDualScreen(platform)){
  auto& db=databases()[platform];loadDatabase(db,platformInfo(platform).slug);
  std::ifstream rom(path,std::ios::binary);char serial[4]{};rom.seekg(platform==Platform::GBA?0xac:0x0c);rom.read(serial,4);
  auto found=db.serials.find(std::string(serial,4));
  if(rom&&found!=db.serials.end()){
   const std::string title=gameTitle(path);bool same=false;
   for(auto& candidate:found->second)if(candidate==title)same=true;
   if(!same){base=found->second.front();for(auto& candidate:found->second)if(candidate.find("(Rev ")==std::string::npos){base=candidate;break;}}
  }
 }
 return resolved[path]=base;
}
// Loose name for matching your own covers: bracketed tags, accents, case, punctuation,
// version numbers ("v2.1", "3.0") and words like "hack" or "version" are ignored, so
// "Pokemon_Unbound_v2.1" and "Pokémon Unbound (Hack)" agree but "Pokemon Emerald" and
// "Pokemon Emerald Rogue" do not.
std::vector<std::string> looseNameWords(const std::string& name){
 static const char* skip[]={"the","version","ver","hack","romhack","rom","patched","patch","beta","alpha","final","fixed"};
 std::vector<std::string> words;std::string word;int depth=0;
 auto flush=[&]{if(word.empty())return;if(word=="pokmon")word="pokemon";bool digits=word.find_first_not_of("0123456789")==std::string::npos;bool v=word.size()>1&&word[0]=='v'&&word.find_first_not_of("0123456789",1)==std::string::npos;bool skipped=digits||v;for(auto* s:skip)if(word==s)skipped=true;if(!skipped)words.push_back(word);word.clear();};
 for(size_t i=0;i<name.size();++i){
  unsigned char c=name[i];
  if(c=='('||c=='['||c=='{'){flush();++depth;continue;}
  if(c==')'||c==']'||c=='}'){if(depth)--depth;continue;}
  if(depth)continue;
  if(c==0xc3&&i+1<name.size()&&((unsigned char)name[i+1]==0xa9||(unsigned char)name[i+1]==0x89)){word+='e';++i;continue;}
  if(c<128&&std::isalnum(c))word+=char(std::tolower(c));else flush();
 }
 flush();
 return words;
}
std::string looseNameKey(const std::string& name){
 std::string key;for(auto& w:looseNameWords(name))key+=w;return key;
}
static size_t editDistance(const std::string& a,const std::string& b){
 std::vector<size_t> row(b.size()+1);for(size_t j=0;j<=b.size();++j)row[j]=j;
 for(size_t i=1;i<=a.size();++i){size_t diag=row[0];row[0]=i;for(size_t j=1;j<=b.size();++j){size_t up=row[j];row[j]=std::min({row[j]+1,row[j-1]+1,diag+(a[i-1]==b[j-1]?0:1)});diag=up;}}
 return row[b.size()];
}
// Picks the cover whose name best fits the game, in order of confidence:
// 1. same loose name; 2. a small typo (up to 2 letters, names of 10+ letters);
// 3. every distinctive word of the cover's name appears in the game's name
//    ("Pokemon" does not count), with at least two such words and only one cover fitting.
// Returns the index in `covers`, or -1.
int matchCover(const std::vector<std::string>& gameNames,const std::vector<std::string>& covers){
 std::vector<std::string> keys;for(auto& c:covers)keys.push_back(looseNameKey(c));
 std::vector<std::string> gameKeys;for(auto& g:gameNames){auto k=looseNameKey(g);if(!k.empty())gameKeys.push_back(k);}
 for(auto& g:gameKeys)for(size_t i=0;i<keys.size();++i)if(!keys[i].empty()&&keys[i]==g)return int(i);
 int best=-1;size_t bestDistance=3;
 for(auto& g:gameKeys)for(size_t i=0;i<keys.size();++i){if(keys[i].size()<10||g.size()<10)continue;size_t d=editDistance(keys[i],g);if(d<bestDistance){bestDistance=d;best=int(i);}}
 if(best>=0)return best;
 auto distinctive=[](std::vector<std::string> w){w.erase(std::remove(w.begin(),w.end(),std::string("pokemon")),w.end());return w;};
 int only=-1,fits=0;
 for(size_t i=0;i<covers.size();++i){
  auto cw=distinctive(looseNameWords(covers[i]));if(cw.size()<2)continue;
  for(auto& g:gameNames){auto gw=looseNameWords(g);bool all=true;for(auto& w:cw)if(std::find(gw.begin(),gw.end(),w)==gw.end())all=false;if(all){++fits;only=int(i);break;}}
 }
 return fits==1?only:-1;
}
// True when the resolved title is a release in the bundled No-Intro table
// (or the sibling GB/GBC table), rather than a filename fallback.
bool gameRecognized(const std::string& path){
 auto title=gameTitle(path);auto platform=detectPlatform(path);
 auto& db=databases()[platform];loadDatabase(db,platformInfo(platform).slug);if(db.exact.count(title))return true;
 if(platform==Platform::GB||platform==Platform::GBC){auto other=platform==Platform::GB?Platform::GBC:Platform::GB;auto& fallback=databases()[other];loadDatabase(fallback,platformInfo(other).slug);return fallback.exact.count(title)>0;}
 return false;
}
