#include "platform_library.h"
#include "dual_screen.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
int main(){assert(detectPlatform("test.GBA")==Platform::GBA);assert(detectPlatform("x.gb")==Platform::GB);assert(detectPlatform("x.gbc")==Platform::GBC);assert(detectPlatform("x.nes")==Platform::NES);assert(detectPlatform("x.sfc")==Platform::SNES);assert(detectPlatform("x.smd")==Platform::Genesis);assert(detectPlatform("x.sms")==Platform::MasterSystem);assert(detectPlatform("x.gg")==Platform::GameGear);assert(detectPlatform("x.nds")==Platform::DS);assert(detectPlatform("roms/dsi/x.nds")==Platform::DSi);assert(detectPlatform("Mario.Z64")==Platform::N64);assert(detectPlatform("Zelda.v64")==Platform::N64);assert(detectPlatform("Mario.n64")==Platform::N64);assert(platforms().size()==11);assert(int(Platform::DSi)==9);assert(int(Platform::N64)==10);assert(gameSavePrefix("build/platform-fixture/","build/platform-fixture/roms/n64/same.z64").find("saves/n64/")!=std::string::npos);assert(detectPlatform("x.3ds")==Platform::Unknown);assert(detectPlatform("x.zip")==Platform::Unknown);assert(detectPlatform("x.bin")==Platform::Unknown);
 fs::create_directories("build/platform-fixture/roms/nes");fs::create_directories("build/platform-fixture/roms/snes");std::ofstream("build/platform-fixture/roms/nes/same.nes");std::ofstream("build/platform-fixture/roms/snes/same.sfc");auto all=scanLibrary("build/platform-fixture/");assert(all.size()==2);assert(filterLibrary(all,4).size()==1);assert(filterLibrary(all,5).size()==1);assert(gameSavePrefix("build/platform-fixture/",all[0])!=gameSavePrefix("build/platform-fixture/",all[1]));
 for(int layout=0;layout<3;++layout){auto screen=dualScreenLayout(layout);float x=0,y=0;assert(bottomScreenPoint(screen.bottom,screen.bottom.x+screen.bottom.w/2,screen.bottom.y+screen.bottom.h/2,x,y));assert(std::abs(x-128)<1&&std::abs(y-96)<1);assert(!bottomScreenPoint(screen.bottom,-1,-1,x,y));}
 fs::create_directories("build/platform-fixture/saves");std::ofstream("build/platform-fixture/saves/legacy.gba.sav");assert(gameSavePrefix("build/platform-fixture/","build/platform-fixture/roms/legacy.gba")=="build/platform-fixture/saves/legacy.gba");assert(gameSavePrefix("build/platform-fixture/","build/platform-fixture/roms/nested/legacy.gba")!="build/platform-fixture/saves/legacy.gba");
 std::cout<<"PASS: platform detection, filters, save isolation and all DS touch layouts\n";
}
