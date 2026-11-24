#include "update_install.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
static std::string failFrom;
static int simulateFailure(const char* from,const char* to){if(failFrom==from){errno=EACCES;return -1;}return std::rename(from,to);}
static void write(const std::string& p,char fill,bool valid=true){std::string data(128,fill);if(valid)data.replace(16,4,"NRO0");std::ofstream(p,std::ios::binary).write(data.data(),data.size());}
static char first(const std::string& p){std::ifstream f(p,std::ios::binary);return char(f.get());}
int main(){fs::create_directories("build/update-install-test");std::string root="build/update-install-test/",old=root+"app.nro",next=root+"app.update",bak=root+"app.bak";
 write(old,'a');write(next,'b',false);assert(!installNroUpdate(old.c_str(),next.c_str(),bak.c_str()));assert(first(old)=='a');
 write(next,'b');failFrom=next;assert(!installNroUpdate(old.c_str(),next.c_str(),bak.c_str(),simulateFailure));assert(first(old)=='a');
 failFrom="";assert(installNroUpdate(old.c_str(),next.c_str(),bak.c_str()));assert(first(old)=='b'&&first(bak)=='a');
 assert(!installNroUpdate(old.c_str(),next.c_str(),bak.c_str()));assert(first(old)=='b'&&first(bak)=='a');
 std::cout<<"PASS: install, backup, rollback on rename failure, invalid/missing download retains app\n";
 std::string fresh=root+"new-core.nro";std::remove(fresh.c_str());write(next,'n');assert(!installNroUpdate(fresh.c_str(),next.c_str(),bak.c_str()));assert(installNroUpdate(fresh.c_str(),next.c_str(),bak.c_str(),std::rename,true));assert(first(fresh)=='n');
}
