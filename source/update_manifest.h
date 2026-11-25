#pragma once
#include <string>
#include <regex>
#include <cstdio>
#include <sstream>
#include <vector>
struct UpdateManifest { std::string version,path,sha256; size_t size=0; };
inline bool parseUpdateManifest(const std::string& json,UpdateManifest& out,bool n64=false,bool ds=false){
 if((n64&&ds)||json.size()>4096)return false;
 auto field=[&](const char* key,const char* pattern,std::string& value){
  std::regex r(std::string("\"")+key+"\"\\s*:\\s*"+pattern);std::sregex_iterator it(json.begin(),json.end(),r),end;
  if(it==end)return false;value=(*it)[1].str();return ++it==end;
 };
 std::string size;
 if(!field("version","\"([0-9]{1,4}\\.[0-9]{1,4}\\.[0-9]{1,4})\"",out.version)||!field("path",(ds?"\"(/releases/[0-9.]+/cores/melonds\\.nro)\"":n64?"\"(/releases/[0-9.]+/cores/mupen64plus-next\\.nro)\"":"\"(/releases/[0-9.]+/AI1Emu\\.nro)\""),out.path)||!field("sha256","\"([a-f0-9]{64})\"",out.sha256)||!field("size","([0-9]{1,9})(?=[,}\\s])",size))return false;
 try{out.size=std::stoul(size);}catch(...){return false;}
 return out.size>=4096&&out.size<=((n64||ds)?64:24)*1024*1024&&out.path=="/releases/"+out.version+(ds?"/cores/melonds.nro":n64?"/cores/mupen64plus-next.nro":"/AI1Emu.nro");
}
// v0.5 emulator file list ("cores.txt"), one line per file after the version line:
//   version 0.5.0
//   fceumm.nro <size> <sha256> /releases/0.5.0/cores/fceumm.nro
struct CoreUpdate { std::string name,path,sha256; size_t size=0; };
struct CoreList { std::string version; std::vector<CoreUpdate> cores; };
inline bool parseCoreList(const std::string& text,CoreList& out){
 static const char* allowed[]={"fceumm.nro","snes9x.nro","genesis-plus-gx.nro","melonds.nro","mupen64plus-next.nro","dekopon.nro"};
 if(text.size()>4096)return false;
 out=CoreList{};std::istringstream in(text);std::string line;
 while(std::getline(in,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
  std::istringstream fields(line);
  if(out.version.empty()){std::string key;if(!(fields>>key>>out.version)||key!="version"||!std::regex_match(out.version,std::regex("[0-9]{1,4}\\.[0-9]{1,4}\\.[0-9]{1,4}")))return false;continue;}
  CoreUpdate c;if(!(fields>>c.name>>c.size>>c.sha256>>c.path))return false;
  bool known=false;for(auto* a:allowed)known|=c.name==a;
  for(auto& seen:out.cores)if(seen.name==c.name)return false;
  if(!known||c.size<4096||c.size>64u*1024*1024||!std::regex_match(c.sha256,std::regex("[a-f0-9]{64}"))||c.path!="/releases/"+out.version+"/cores/"+c.name)return false;
  out.cores.push_back(c);
 }
 return !out.version.empty()&&!out.cores.empty();
}
inline bool newerVersion(const std::string& candidate,const std::string& current){
 unsigned a=0,b=0,c=0,x=0,y=0,z=0;
 if(std::sscanf(candidate.c_str(),"%u.%u.%u",&a,&b,&c)!=3||std::sscanf(current.c_str(),"%u.%u.%u",&x,&y,&z)!=3)return false;
 return a!=x?a>x:b!=y?b>y:c>z;
}
