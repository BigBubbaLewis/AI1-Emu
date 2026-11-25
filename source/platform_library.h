// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <vector>
enum class Platform { Unknown=-1,GBA=0,GB,GBC,NES,SNES,Genesis,MasterSystem,GameGear,DS,DSi,N64,N3DS };
struct PlatformInfo {Platform id;const char* slug;const char* name;const char* shortName;const char* core;};
const std::vector<PlatformInfo>& platforms();
const PlatformInfo& platformInfo(Platform platform);
Platform detectPlatform(const std::string& path);
std::vector<std::string> scanLibrary(const std::string& root);
// Dekopon's ROM folders (default roms/ plus roms_dir/roms_dir_2 from its config.ini).
std::vector<std::string> dekoponRomFolders(const std::string& dekoponRoot);
// True when two game files are copies of each other. Files above fullCompareLimit are
// compared at evenly spread sample blocks instead of in full (multi-GB 3DS games).
bool sameRomContent(const std::string& a, const std::string& b, std::uintmax_t fullCompareLimit);
// libretro-thumbnails repository holding this console's box art; nullptr when none.
const char* thumbnailRepository(Platform p);
std::vector<std::string> filterLibrary(const std::vector<std::string>& all,int category);
std::string gameSavePrefix(const std::string& root,const std::string& game);
bool isDualScreen(Platform platform);
// Consoles that need full-memory application mode (not the album applet).
bool needsFullMemory(Platform platform);
