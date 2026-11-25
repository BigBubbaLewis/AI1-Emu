// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
struct CheatCandidate { std::string id,title,sha;size_t bytes=0;unsigned count=0; };
bool parseCheatCatalog(const std::string& text,std::vector<CheatCandidate>& out);
class OnlineCheats {
public:
 enum Phase {Idle,Searching,Choices,Downloading,Ready,Failed};
 ~OnlineCheats();
 void search(const std::string& platform,const std::string& title);
 void download(const CheatCandidate& entry,const std::string& cacheRoot);
 Phase phase() const{return state.load();}
 std::string message();std::string cachedFile();std::vector<CheatCandidate> candidates();
 void cancel(){cancelled=true;}
private:
 std::atomic<Phase> state{Idle};std::atomic<bool> cancelled{false};SDL_Thread* thread=nullptr;std::mutex mutex;
 std::string detail,requestPath,cachePath;CheatCandidate selected;std::vector<CheatCandidate> matches;bool downloading=false;
 void set(Phase,const std::string&);void start();void run();static int entry(void*);bool fetch(std::string&,size_t);
};
