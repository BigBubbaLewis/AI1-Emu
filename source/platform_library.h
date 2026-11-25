// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
enum class Platform { Unknown=-1,GBA=0,GB,GBC,NES,SNES,Genesis,MasterSystem,GameGear,DS,DSi,N64,N3DS };
struct PlatformInfo {Platform id;const char* slug;const char* name;const char* shortName;const char* core;};
const std::vector<PlatformInfo>& platforms();
const PlatformInfo& platformInfo(Platform platform);
Platform detectPlatform(const std::string& path);
std::vector<std::string> scanLibrary(const std::string& root);
std::vector<std::string> filterLibrary(const std::vector<std::string>& all,int category);
std::string gameSavePrefix(const std::string& root,const std::string& game);
bool isDualScreen(Platform platform);
// Consoles that need full-memory application mode (not the album applet).
bool needsFullMemory(Platform platform);
