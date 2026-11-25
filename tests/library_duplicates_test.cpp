#include "platform_library.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cassert>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
static void write(const fs::path& p,const std::vector<char>& data){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary).write(data.data(),data.size());}
int main(){std::string root="build/duplicate-fixture-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+"/";std::vector<char> data(65536,'r');auto a=root+"roms/Super Metroid.sfc",b=root+"roms/snes/Renamed Copy.sfc",c=root+"roms/snes/Different Revision.sfc";write(a,data);write(b,data);data[8192]='x';write(c,data);data[0x143]=0;write(root+"roms/gb/Same.gb",data);write(root+"roms/gbc/Same.gbc",data);write(root+"roms/empty.nes",{});auto saved=gameSavePrefix(root,b)+".sav";write(saved,{'s','a','v','e'});write(gameSavePrefix(root,a)+".sav",{'o','l','d'});write(gameSavePrefix(root,a)+".lastplayed",{'1','0','0'});write(gameSavePrefix(root,b)+".lastplayed",{'2','0','0'});auto all=scanLibrary(root);assert(all.size()==5);assert(filterLibrary(all,5).size()==2);assert(std::any_of(all.begin(),all.end(),[&](auto& p){return fs::path(p).generic_string()==fs::path(b).generic_string();}));assert(!std::any_of(all.begin(),all.end(),[&](auto& p){return fs::path(p).generic_string()==fs::path(a).generic_string();}));assert(fs::exists(a)&&fs::exists(b));std::ifstream f(saved);std::string content;f>>content;assert(content=="save");assert(scanLibrary(root)==all);{const std::string d="build/big-rom-compare/";fs::remove_all(d);fs::create_directories(d);
  std::vector<char> data(1024*1024);for(size_t i=0;i<data.size();++i)data[i]=char((i*131+7)&0xff);
  auto save=[&](const std::string& n,const std::vector<char>& v){std::ofstream(d+n,std::ios::binary).write(v.data(),v.size());};
  save("a.cci",data);save("b.cci",data);
  assert(sameRomContent(d+"a.cci",d+"b.cci",64*1024)); // sampled path: identical copies match
  assert(sameRomContent(d+"a.cci",d+"b.cci",64ull*1024*1024)); // full path
  auto first=data;first[0]^=1;save("c.cci",first);assert(!sameRomContent(d+"a.cci",d+"c.cci",64*1024));
  auto last=data;last.back()^=1;save("e.cci",last);assert(!sameRomContent(d+"a.cci",d+"e.cci",64*1024));
  auto middle=data;middle[300001]^=1;save("f.cci",middle);assert(!sameRomContent(d+"a.cci",d+"f.cci",64ull*1024*1024)); // small games: full check
  auto shorter=data;shorter.pop_back();save("g.cci",shorter);assert(!sameRomContent(d+"a.cci",d+"g.cci",64*1024));
  fs::remove_all(d);}
 std::cout<<"PASS: renamed identical copies collapse, existing save preferred, full compare rejects sample collision, distinct console/revision preserved, files unchanged and repeated scan stable\n";}
