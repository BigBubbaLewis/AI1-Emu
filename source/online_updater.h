#pragma once
#include <SDL.h>
#include <atomic>
#include <mutex>
#include <string>
#include "update_manifest.h"
#include "app_version.h"
#include <curl/curl.h>
// Checks for a newer AI1Emu.nro; when the app is current, also checks the
// emulator files (cores.txt) and installs those in place without closing the app.
// The test channel (update-channel.txt containing "test") uses a separate service.
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
 static bool testChannel();
 static std::string installedCores(); // version recorded in cores/cores.version
private:
 std::atomic<Phase> state{Idle};
 std::atomic<bool> cancel{false};
 std::atomic<unsigned> percent{0};
 std::mutex mutex;
 std::string detail;
 std::string fetchFailure;
 UpdateManifest manifest;
 CoreList coreList;
 SDL_Thread* thread=nullptr;
 bool fetchingFile=false,n64=false,ds=false,coresMode=false,test=false;
 std::string origin=AI1EMU_UPDATE_ORIGIN;
 std::string target="sdmc:/switch/ai1emu/AI1Emu.nro",staged=target+".update",backup=target+".bak";
 void start(bool file);
 void run();
 void runCores();
 bool fetch(const std::string& path,std::string* text,const char* file,size_t limit,std::string* sha,long timeout);
 void set(Phase phase,const std::string& message);
 static int entry(void* data);
 static int progress(void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t);
};
