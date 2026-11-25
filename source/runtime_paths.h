#pragma once
#include <string>
#include <filesystem>
#include <cctype>
#ifdef RETROVERSE_WINDOWS_PREVIEW
#include <cstdlib>
inline std::string retroverseRoot(){const char* p=std::getenv("AI1EMU_PREVIEW_ROOT");if(!p)p=std::getenv("RETROVERSE_PREVIEW_ROOT");return p?std::string(p)+"/":"data/";}
inline std::string retroverseAssets(){return "romfs/";}
#else
inline std::string retroverseRoot(){
 static const std::string chosen=[]()->std::string{
 // Continue using the existing data folder until the user migrates it.
 // Never move or duplicate ROMs or saves during startup.
 std::error_code error;
 auto hasGames=[&](const char* folder){
  std::filesystem::recursive_directory_iterator end,it(folder,std::filesystem::directory_options::skip_permission_denied,error);
  for(;!error&&it!=end;it.increment(error)){auto ext=it->path().extension().string();for(char& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));if(ext==".gba"||ext==".gb"||ext==".gbc"||ext==".nes"||ext==".sfc"||ext==".smc"||ext==".md"||ext==".gen"||ext==".smd"||ext==".sms"||ext==".gg"||ext==".nds"||ext==".z64"||ext==".v64"||ext==".n64"||ext==".dsi"||ext==".3ds"||ext==".cci"||ext==".cxi"||ext==".zcci"||ext==".zcxi"||ext==".bin")return true;}
  error.clear();return false;
 };
 if(!hasGames("sdmc:/switch/ai1emu/roms")&&hasGames("sdmc:/switch/retroverse/roms"))return "sdmc:/switch/retroverse/";
 return "sdmc:/switch/ai1emu/";
 }();return chosen;
}
inline std::string retroverseAssets(){return "romfs:/";}
#endif

