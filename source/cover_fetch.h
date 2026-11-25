// SPDX-License-Identifier: MIT
#pragma once
#include <SDL.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
// Downloads missing box art from libretro-thumbnails in the background, one cover at
// a time. A cover that does not exist online gets a "<dest>.none" note so it is not
// asked for again; a network failure stops the run quietly until the next launch.
class CoverFetcher {
public:
 struct Job { std::string url, dest; }; // dest: cover path without extension
 ~CoverFetcher();
 void add(Job job);
 std::vector<std::string> takeFinished(); // dests saved since the last call
 bool offline() const { return failed.load(); }
private:
 std::mutex mutex;
 std::condition_variable wake;
 std::deque<Job> queue;
 std::vector<std::string> finished;
 std::atomic<bool> stop{false}, failed{false};
 SDL_Thread* thread = nullptr;
 static int entry(void* self);
 void run();
};
