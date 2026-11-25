// SPDX-License-Identifier: MIT
#pragma once
#include <cstdio>
#include <cerrno>
#include <string>
#include <filesystem>
// Preserve the previous file until the replacement is closed and installed.
inline bool safeReplace(const std::string& staged,const std::string& target,int(*renameFile)(const char*,const char*)=std::rename){
 const auto backup=target+".previous";std::error_code ec;
 bool exists=std::filesystem::exists(target,ec);if(ec)return false;
 if(!exists&&std::filesystem::exists(backup,ec)){if(renameFile(backup.c_str(),target.c_str())!=0)return false;exists=true;}
 if(ec)return false;
 if(exists){if(std::remove(backup.c_str())!=0&&errno!=ENOENT)return false;if(renameFile(target.c_str(),backup.c_str())!=0)return false;}
 if(renameFile(staged.c_str(),target.c_str())!=0){if(exists)renameFile(backup.c_str(),target.c_str());return false;}
 return true;
}
inline bool safeWriteFile(const std::string& target,const void* data,size_t size){
 std::error_code ec;const auto parent=std::filesystem::path(target).parent_path();if(!parent.empty())std::filesystem::create_directories(parent,ec);if(ec)return false;
 const auto staged=target+".tmp";FILE* out=std::fopen(staged.c_str(),"wb");if(!out)return false;
 bool ok=std::fwrite(data,1,size,out)==size;if(std::fflush(out)!=0)ok=false;
 if(std::fclose(out)!=0)ok=false;if(!ok){std::remove(staged.c_str());return false;}
 return safeReplace(staged,target);
}
