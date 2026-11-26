#define SDL_MAIN_HANDLED
#include "safe_file.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static int failingRename(const char* from,const char* to){if(std::string(from).find(".tmp")!=std::string::npos){errno=EIO;return -1;}return std::rename(from,to);}
static std::string content(const std::string& file){std::ifstream f(file);return {std::istreambuf_iterator<char>(f),{}};}
int main(){const std::string dir="build/tests/safe-file-fixture",file=dir+"/settings.txt";fs::create_directories(dir);assert(safeWriteFile(file,"old",3));assert(safeWriteFile(file+".tmp","new",3));assert(!safeReplace(file+".tmp",file,failingRename));assert(content(file)=="old");assert(safeWriteFile(file,"updated",7));assert(content(file)=="updated");assert(content(file+".previous")=="old");std::remove(file.c_str());assert(safeWriteFile(file,"recovered",9));assert(content(file)=="recovered");assert(content(file+".previous")=="old");{const std::string sav="build/newest-save.sav",tmp=sav+".tmp";std::remove(sav.c_str());std::remove(tmp.c_str());std::remove((sav+".previous").c_str());
 assert(newestSave(sav)==sav); // nothing there yet
 std::ofstream(sav,std::ios::binary)<<"first";assert(newestSave(sav)==sav); // no stuck copy
 std::ofstream(tmp,std::ios::binary)<<"newest";
 std::filesystem::last_write_time(sav,std::filesystem::last_write_time(tmp)-std::chrono::hours(1));
 assert(newestSave(sav)==tmp); // stuck newer progress is preferred
 std::ofstream(tmp,std::ios::binary);assert(newestSave(sav)==sav); // an empty leftover is ignored
 std::ofstream(tmp,std::ios::binary)<<"older";std::filesystem::last_write_time(tmp,std::filesystem::last_write_time(sav)-std::chrono::hours(1));
 assert(newestSave(sav)==sav); // an older leftover is ignored
 assert(safeWriteFile(sav,"saved",5)&&content(sav)=="saved"); // writing over an existing save works
}
std::cout<<"PASS: failed install rolls back; previous content survives replacement and interrupted target rename; newest save recovered from a stuck .tmp\n";}
