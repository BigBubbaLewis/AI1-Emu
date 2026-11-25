#define SDL_MAIN_HANDLED
#include "safe_file.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static int failingRename(const char* from,const char* to){if(std::string(from).find(".tmp")!=std::string::npos){errno=EIO;return -1;}return std::rename(from,to);}
static std::string content(const std::string& file){std::ifstream f(file);return {std::istreambuf_iterator<char>(f),{}};}
int main(){const std::string dir="build/tests/safe-file-fixture",file=dir+"/settings.txt";fs::create_directories(dir);assert(safeWriteFile(file,"old",3));assert(safeWriteFile(file+".tmp","new",3));assert(!safeReplace(file+".tmp",file,failingRename));assert(content(file)=="old");assert(safeWriteFile(file,"updated",7));assert(content(file)=="updated");assert(content(file+".previous")=="old");std::remove(file.c_str());assert(safeWriteFile(file,"recovered",9));assert(content(file)=="recovered");assert(content(file+".previous")=="old");std::cout<<"PASS: failed install rolls back; previous content survives replacement and interrupted target rename\n";}
