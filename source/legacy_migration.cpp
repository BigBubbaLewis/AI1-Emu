// SPDX-License-Identifier: MIT
#include "legacy_migration.h"
#include "platform_library.h"
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <cstdint>
namespace fs=std::filesystem;
namespace {
struct Cancelled {};
std::string lower(std::string s){for(char& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
bool validNro(const fs::path& p){std::ifstream f(p,std::ios::binary);char magic[4]{};f.seekg(16);f.read(magic,4);return f&&std::string(magic,4)=="NRO0"&&fs::file_size(p)>4096;}
// Same naming rule as platform_library.cpp, but use the source cartridge's
// detected platform before its destination file exists (important for .gb).
std::string targetPrefix(const std::string& root,const fs::path& game,Platform platform){
 std::string file=game.filename().string(),legacy=root+"saves/"+file;
 if(platform==Platform::GBA&&game.parent_path().lexically_normal()==fs::path(root+"roms").lexically_normal())for(const char* suffix:{".sav",".state",".lastplayed"})if(fs::exists(legacy+suffix))return legacy;
 std::error_code error;std::string relative=fs::relative(game,root+"roms",error).generic_string();if(error)relative=game.string();
 uint64_t hash=14695981039346656037ULL;for(unsigned char c:relative){hash^=c;hash*=1099511628211ULL;}std::ostringstream suffix;suffix<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
 return root+"saves/"+platformInfo(platform).slug+"/"+file+"."+suffix.str();
}
}
MigrationResult mergeLegacyFolder(const fs::path& switchDirectory,const std::function<bool(const std::string&)>& progress){
 MigrationResult result;const auto base=switchDirectory.lexically_normal(),old=base/"retroverse",current=base/"ai1emu";
 if(!fs::exists(old))return result;result.attempted=true;
 std::string currentFile="CHECKING OLD FOLDER";std::ofstream report;
 std::vector<char> a(256*1024),b(256*1024);
 auto poll=[&]{if(progress&&!progress(currentFile))throw Cancelled{};};
 try{
  if(fs::is_symlink(old)||fs::is_symlink(current)||!fs::is_directory(old)||!validNro(current/"AI1Emu.nro"))throw std::runtime_error("Install the AI1 app before merging the old folder.");
#ifndef __SWITCH__
  auto resolvedBase=fs::weakly_canonical(base);if(fs::weakly_canonical(old).parent_path()!=resolvedBase||fs::weakly_canonical(current).parent_path()!=resolvedBase)throw std::runtime_error("Migration folders resolve outside the switch directory.");
#endif
  fs::create_directories(current);report.open(current/"migration-report.txt",std::ios::app);if(!report)throw std::runtime_error("Cannot write migration report.");
  report<<"\nAI1 Emu legacy merge started\n";report.flush();
  auto same=[&](const fs::path& x,const fs::path& y){
   if(fs::is_symlink(x)||fs::is_symlink(y))throw std::runtime_error("A symbolic link was found; old folder kept.");
   if(fs::file_size(x)!=fs::file_size(y))return false;
   std::ifstream left(x,std::ios::binary),right(y,std::ios::binary);if(!left||!right)throw std::runtime_error("Cannot read a file for verification.");
   while(left){poll();left.read(a.data(),a.size());right.read(b.data(),b.size());auto n=left.gcount();if(n!=right.gcount()||!std::equal(a.begin(),a.begin()+n,b.begin()))return false;}
   if(left.bad()||right.bad())throw std::runtime_error("SD read failed during verification.");return true;
  };
  // Reject linked entries before doing any destructive work. Directory traversal
  // never follows symlinks, and cleanup below only removes empty directories.
  std::vector<fs::path> files,dirs;
  for(auto& entry:fs::recursive_directory_iterator(old)){
   if(entry.is_symlink())throw std::runtime_error("A symbolic link was found; old folder kept.");
   if(entry.is_directory())dirs.push_back(entry.path());else if(entry.is_regular_file())files.push_back(entry.path());else throw std::runtime_error("Unsupported entry in old folder.");
  }
  std::map<std::string,std::vector<fs::path>> candidates;
  if(fs::exists(current/"roms"))for(auto& entry:fs::recursive_directory_iterator(current/"roms"))if(!entry.is_symlink()&&entry.is_regular_file()&&detectPlatform(entry.path().string())!=Platform::Unknown)candidates[lower(entry.path().filename().string())].push_back(entry.path());
  std::map<fs::path,fs::path> duplicateRoms;
  std::vector<std::pair<std::string,std::string>> saveRemaps;
  for(auto& file:files){auto relative=file.lexically_relative(old);auto platform=detectPlatform(file.string());if(relative.empty()||*relative.begin()!="roms"||platform==Platform::Unknown)continue;
   currentFile="CHECKING ROM DUPLICATES";poll();
   auto found=candidates.find(lower(file.filename().string()));if(found!=candidates.end())for(auto& existing:found->second)if(detectPlatform(existing.string())==platform&&same(file,existing)){
    duplicateRoms[file]=existing;
    break;
   }
   auto oldPrefix=fs::path(gameSavePrefix(old.generic_string()+"/",file.string())).lexically_relative(old).generic_string();
   auto match=duplicateRoms.find(file);auto target=match==duplicateRoms.end()?current/relative:match->second;
   bool conflictingRom=match==duplicateRoms.end()&&fs::exists(target);
   auto newPrefix=conflictingRom?"legacy-conflicts/"+oldPrefix:fs::path(targetPrefix(current.generic_string()+"/",target,platform)).lexically_relative(current).generic_string();
   saveRemaps.emplace_back(oldPrefix,newPrefix);
  }
  // ROMs go last, so interrupted merges retain the old cartridge paths until
  // their saves have been copied/remapped. No existing AI1 save is overwritten.
  std::stable_sort(files.begin(),files.end(),[&](const auto& x,const auto& y){return (x.lexically_relative(old).begin()->string()=="roms")<(y.lexically_relative(old).begin()->string()=="roms");});
  std::vector<fs::path> obsolete;
  size_t index=0;
  for(auto& file:files){
   auto relative=file.lexically_relative(old);if(relative.empty()||*relative.begin()=="..")throw std::runtime_error("Unsafe migration path.");
   currentFile="MERGING FILE "+std::to_string(++index)+" OF "+std::to_string(files.size());poll();
   auto filename=lower(relative.filename().string());
   if(relative.parent_path().empty()&&(filename=="retroverse.nro"||filename=="ai1emu.nro")){obsolete.push_back(file);continue;}
   auto rom=duplicateRoms.find(file);fs::path destination=current/relative;
   if(rom!=duplicateRoms.end())destination=rom->second;
   else{
    auto key=relative.generic_string();for(auto& mapping:saveRemaps){auto suffix=key.substr(std::min(key.size(),mapping.first.size()));if(key.rfind(mapping.first,0)==0&&(suffix==".sav"||suffix==".state"||suffix==".lastplayed"||suffix.rfind(".files/",0)==0)){destination=current/(mapping.second+suffix);break;}}
   }
   auto checkDestination=[&]{auto checked=current;for(auto& component:destination.lexically_relative(current)){if(component=="..")throw std::runtime_error("Unsafe destination path.");checked/=component;if(fs::is_symlink(checked))throw std::runtime_error("Linked destination found; source kept.");}};checkDestination();
   auto removeSource=[&]{if(!fs::remove(file))throw std::runtime_error("Could not remove verified source file.");};
   if(fs::exists(destination)){
    if(!fs::is_regular_file(destination)||fs::is_symlink(destination))throw std::runtime_error("Destination conflict is not a regular file.");
    if(same(file,destination)){auto bytes=fs::file_size(file);removeSource();result.duplicates++;result.freed+=bytes;report<<"DUPLICATE "<<relative.generic_string()<<" -> "<<destination.lexically_relative(current).generic_string()<<"\n";report.flush();continue;}
    destination=current/"legacy-conflicts"/relative;checkDestination();size_t count=0;
    while(fs::exists(destination)&&!same(file,destination)){destination=current/"legacy-conflicts"/relative;destination+= "."+std::to_string(++count);checkDestination();}
    result.conflicts++;
    if(fs::exists(destination)){removeSource();result.duplicates++;report<<"PRESERVED ALREADY "<<relative.generic_string()<<"\n";report.flush();continue;}
   }
   // Copy to a new temporary name, compare every byte, then rename and remove
   // the original. A copy failure/cancel leaves the source intact.
   fs::create_directories(destination.parent_path());auto temporary=destination;size_t number=0;
   do{temporary=destination;temporary+=".ai1-migrate-"+std::to_string(++number)+".tmp";}while(fs::exists(temporary));
   try{
    std::ifstream input(file,std::ios::binary);std::ofstream output(temporary,std::ios::binary);if(!input||!output)throw std::runtime_error("Cannot create migration copy. Check free SD space.");
    while(input){poll();input.read(a.data(),a.size());output.write(a.data(),input.gcount());if(!output)throw std::runtime_error("SD write failed. Old source kept.");}
    bool readFailed=input.bad();input.close();output.close();if(readFailed||!output||!same(file,temporary))throw std::runtime_error("Copy verification failed. Old source kept.");
    fs::rename(temporary,destination);removeSource();result.copied++;
    report<<"MOVED "<<relative.generic_string()<<" -> "<<destination.lexically_relative(current).generic_string()<<"\n";report.flush();
   }catch(...){std::error_code ignored;fs::remove(temporary,ignored);throw;}
  }
  for(auto& file:obsolete){result.freed+=fs::file_size(file);if(!fs::remove(file))throw std::runtime_error("Could not remove obsolete old app.");report<<"REMOVED OBSOLETE APP "<<file.filename().string()<<"\n";}
  std::sort(dirs.begin(),dirs.end(),[](const auto& x,const auto& y){return x.native().size()>y.native().size();});
  for(auto& dir:dirs)fs::remove(dir);fs::remove(old);result.complete=!fs::exists(old);
  result.message=result.complete?"Old folder combined into ai1emu.":"Merge incomplete. Old folder kept.";
  if(result.conflicts)result.message+=" Older conflicts kept in legacy-conflicts.";
  report<<result.message<<" Copied="<<result.copied<<" Duplicates="<<result.duplicates<<" Conflicts="<<result.conflicts<<"\n";
 }catch(const Cancelled&){result.message="Merge paused. Original uncopied files kept; restart to continue.";}
 catch(const std::exception& e){result.message="Old folder merge stopped: "+std::string(e.what());}
 if(report)report<<result.message<<"\n";return result;
}
