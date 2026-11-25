// SPDX-License-Identifier: MIT
#include "cover_fetch.h"
#include "app_version.h"
#include "app_log.h"
#include "network.h"
#include "safe_file.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <curl/curl.h>

CoverFetcher::~CoverFetcher() {
 { std::lock_guard<std::mutex> lock(mutex); stop = true; }
 wake.notify_all();
 if (thread) SDL_WaitThread(thread, nullptr);
}

void CoverFetcher::add(Job job) {
 if (failed) return;
 { std::lock_guard<std::mutex> lock(mutex); queue.push_back(std::move(job)); }
 if (!thread) { thread = SDL_CreateThread(entry, "ai1-cover-fetch", this); appLog(thread ? "Covers: download thread started" : "Covers: thread failed"); }
 wake.notify_one();
}

std::vector<std::string> CoverFetcher::takeFinished() {
 std::lock_guard<std::mutex> lock(mutex);
 std::vector<std::string> out;
 out.swap(finished);
 return out;
}

int CoverFetcher::entry(void* self) { static_cast<CoverFetcher*>(self)->run(); return 0; }

void CoverFetcher::run() {
 SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW); // the library screen always comes first
 if (!networkStart()) { failed = true; appLog("Covers: no network, stopping until next launch"); return; }
 unsigned saved = 0, none = 0;
 for (;;) {
  Job job;
  {
   std::unique_lock<std::mutex> lock(mutex);
   wake.wait(lock, [this] { return stop.load() || !queue.empty(); });
   if (stop) return;
   job = std::move(queue.front());
   queue.pop_front();
  }
  struct Sink { std::string data; } sink;
  CURL* curl = curl_easy_init();
  if (!curl) { failed = true; return; }
  curl_easy_setopt(curl, CURLOPT_URL, job.url.c_str());
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "AI1Emu/" AI1EMU_VERSION);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* data, size_t a, size_t b, void* p) -> size_t {
   auto& s = *static_cast<Sink*>(p);
   size_t n = a * b;
   if (s.data.size() + n > 4 * 1024 * 1024) return 0; // covers are small; refuse anything huge
   s.data.append(data, n);
   return n;
  });
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, +[](void* p, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int { return static_cast<CoverFetcher*>(p)->stop ? 1 : 0; });
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
  CURLcode result = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  if (stop) return;
  if (result != CURLE_OK) { failed = true; appLog("Covers: HTTPS error " + std::to_string(int(result)) + " (" + curl_easy_strerror(result) + "), stopping until next launch"); return; }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(job.dest).parent_path(), ec);
  if (status == 404) { std::ofstream(job.dest + ".none") << "No cover on libretro-thumbnails. Add your own in covers/.\n"; appLog("Covers: none online (" + std::to_string(++none) + ")"); continue; }
  static const char png[] = "\x89PNG\r\n\x1a\n";
  if (status != 200 || sink.data.size() < 8 || std::memcmp(sink.data.data(), png, 8) != 0) { appLog("Covers: skipped, HTTP " + std::to_string(status)); continue; }
  if (safeWriteFile(job.dest + ".png", sink.data.data(), sink.data.size())) {
   std::lock_guard<std::mutex> lock(mutex);
   finished.push_back(job.dest);
   ++saved;
  }
  if (saved && saved % 10 == 1) appLog("Covers: " + std::to_string(saved) + " saved so far");
 }
}
