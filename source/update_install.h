#pragma once
#include <cstdio>
#include <cstring>
#include <cerrno>
inline bool installNroUpdate(const char* target,const char* staged,const char* backup,int (*renameFile)(const char*,const char*)=std::rename,bool allowNew=false){
 FILE* old=fopen(target,"rb");bool exists=old!=nullptr;if(old)fclose(old);if(!exists&&!allowNew)return false;
 FILE* next=fopen(staged,"rb");if(!next)return false;unsigned char header[32]{};
 bool valid=fread(header,1,sizeof(header),next)==sizeof(header)&&memcmp(header+16,"NRO0",4)==0;fclose(next);if(!valid)return false;
 if(!exists)return renameFile(staged,target)==0;
 if(std::remove(backup)!=0&&errno!=ENOENT)return false;
 if(renameFile(target,backup)!=0)return false;
 if(renameFile(staged,target)!=0){renameFile(backup,target);return false;}
 return true;
}
