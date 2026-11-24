#define SDL_MAIN_HANDLED
#include "library_ui.h"
#include "platform_library.h"
#include "game_database.h"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
int main(){SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);std::string root="build/yellow-fixture-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());_putenv_s("AI1EMU_PREVIEW_ROOT",root.c_str());std::string title="Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced)";auto rom=root+"/roms/"+title+".gb";fs::create_directories(root+"/roms");std::vector<char> header(512);header[0x143]=char(0x80);std::ofstream(rom,std::ios::binary).write(header.data(),header.size());fs::create_directories(root+"/artwork-cache/gb");fs::copy_file("romfs/app-icon.png",root+"/artwork-cache/gb/"+title+".png");fs::create_directories(root+"/artwork-cache/gbc");std::ofstream(root+"/artwork-cache/gbc/"+title+".png")<<"invalid cached image";assert(detectPlatform(rom)==Platform::GBC);assert(gameTitle(rom)==title);auto surface=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);auto renderer=SDL_CreateSoftwareRenderer(surface);assert(renderer);{LibraryUI ui(renderer);size_t selected=0;ui.draw({rom},selected,false,"",false);auto key=root+"/artwork-cache/gb/"+title;assert(ui.images.count(key));assert(ui.images[key].texture);ui.tab=LibraryUI::TabMap;ui.draw({rom},selected,false,"",false);ui.draw({rom},selected,false,"",false);assert(ui.mapState==2&&ui.mapOwned.size()==1);ui.draw({rom,rom},selected,false,"",false);assert(ui.mapOwned.size()==1);ui.stopStorageScan();}SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();std::cout<<"PASS: enhanced GB header selects GBC while Yellow resolves GB database and renders cached GB cover in actual UI\n";return 0;}
