#include "legacy_migration.h"
#include "platform_library.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
#include <chrono>
namespace fs=std::filesystem;
static void put(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<s;}
static std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void nro(const fs::path& p){std::string s(8192,'\0');s.replace(16,4,"NRO0");put(p,s);}
int main(){
 auto base=fs::absolute(fs::path("build")/("migration-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));auto old=base/"retroverse",current=base/"ai1emu";
 nro(current/"AI1Emu.nro");nro(old/"RetroVerse.nro");
 put(current/"roms/gba/Same.gba","owned game fixture");put(old/"roms/Same.gba","owned game fixture");
 put(old/"saves/Same.gba.sav","old game progress");
 put(old/"roms/nes/Unique.nes","unique cartridge fixture");put(old/"saves/nes/state.state","state fixture");
 put(current/"covers/custom.png","current custom");put(old/"covers/custom.png","old custom");
 put(current/"artwork-cache/cover.png","same cover");put(old/"artwork-cache/cover.png","same cover");
 auto prefix=gameSavePrefix(current.generic_string()+"/",(current/"roms/gba/Same.gba").string());
 auto r=mergeLegacyFolder(base);assert(r.complete&&!fs::exists(old));assert(get(prefix+".sav")=="old game progress");assert(!fs::exists(current/"roms/Same.gba"));assert(get(current/"roms/gba/Same.gba")=="owned game fixture");assert(get(current/"roms/nes/Unique.nes")=="unique cartridge fixture");assert(get(current/"covers/custom.png")=="current custom");assert(get(current/"legacy-conflicts/covers/custom.png")=="old custom");assert(r.duplicates>=2&&r.conflicts==1);assert(!mergeLegacyFolder(base).attempted);
 // An existing AI1 save always wins; the older save survives separately.
 put(old/"roms/Same.gba","owned game fixture");put(old/"saves/Same.gba.sav","different old progress");
 r=mergeLegacyFolder(base);assert(r.complete);assert(get(prefix+".sav")=="old game progress");assert(get(current/"legacy-conflicts/saves/Same.gba.sav")=="different old progress");
 // Cancellation retains old ROMs and can safely resume.
 put(old/"roms/Cancel.gba","cancel cartridge");put(old/"saves/Cancel.gba.sav","cancel save");
 r=mergeLegacyFolder(base,[](const auto&){return false;});assert(!r.complete&&fs::exists(old/"roms/Cancel.gba"));assert(mergeLegacyFolder(base).complete);
 assert(get(gameSavePrefix(current.generic_string()+"/",(current/"roms/Cancel.gba").string())+".sav")=="cancel save");
 std::string ds(512,'\0');put(current/"roms/nds/Dual.nds",ds);put(old/"roms/Dual.nds",ds);
 auto oldDs=gameSavePrefix(old.generic_string()+"/",(old/"roms/Dual.nds").string());auto currentDs=gameSavePrefix(current.generic_string()+"/",(current/"roms/nds/Dual.nds").string());put(oldDs+".files/Dual.sav","DS progress");
 r=mergeLegacyFolder(base);assert(r.complete);assert(get(currentDs+".files/Dual.sav")=="DS progress");assert(!fs::exists(current/"roms/Dual.nds"));
 put(old/"roms/dsi/Dual.nds",ds);auto oldDsi=gameSavePrefix(old.generic_string()+"/",(old/"roms/dsi/Dual.nds").string());put(oldDsi+".files/Dual.sav","DSi progress");
 r=mergeLegacyFolder(base);assert(r.complete&&fs::exists(current/"roms/dsi/Dual.nds"));auto newDsi=gameSavePrefix(current.generic_string()+"/",(current/"roms/dsi/Dual.nds").string());assert(get(newDsi+".files/Dual.sav")=="DSi progress");assert(get(currentDs+".files/Dual.sav")=="DS progress");
 put(old/"roms/Partial.gba",std::string(1024*1024,'p'));
 r=mergeLegacyFolder(base,[&](const auto&){for(auto& entry:fs::recursive_directory_iterator(current))if(entry.is_regular_file()&&entry.path().filename().string().find(".ai1-migrate-")!=std::string::npos&&entry.file_size()>0)return false;return true;});
 assert(!r.complete&&fs::file_size(old/"roms/Partial.gba")==1024*1024&&!fs::exists(current/"roms/Partial.gba"));assert(mergeLegacyFolder(base).complete);
 // Without a valid installed app, nothing is removed.
 auto invalid=base/"invalid";put(invalid/"retroverse/roms/Keep.gba","keep");r=mergeLegacyFolder(invalid);assert(!r.complete&&get(invalid/"retroverse/roms/Keep.gba")=="keep");
 std::cout<<"PASS: verified merge, cross-folder ROM dedup/save remap, conflict preservation, cancel/resume and invalid destination protection\n";
}
