#pragma once
#include <SDL.h>
#include <atomic>
#include <mutex>
#include <string>
#include "update_manifest.h"
#include <curl/curl.h>
class OnlineUpdater {
public:
 enum Phase { Idle,Checking,Current,Available,Downloading,Ready,Failed };
 OnlineUpdater()=default;
 ~OnlineUpdater();
 void check();
 void download();
 Phase phase()const{return state.load();}
 std::string label();
 std::string message();
 bool apply(); // Call only after RomFS closes and all downloads finish.
private:
 std::atomic<Phase> state{Idle};
 std::atomic<bool> cancel{false};
 std::atomic<unsigned> percent{0};
 std::mutex mutex;
 std::string detail;
 UpdateManifest manifest;
 SDL_Thread* thread=nullptr;
 bool sockets=false;
 bool fetchingFile=false,n64=false,ds=false;
 std::string target="sdmc:/switch/ai1emu/AI1Emu.nro",staged=target+".update",backup=target+".bak";
 void start(bool file);
 void run();
 void set(Phase phase,const std::string& message);
 static int entry(void* data);
 static int progress(void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t);
};
