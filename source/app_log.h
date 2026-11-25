// SPDX-License-Identifier: MIT
#pragma once
// Startup / network step log at sdmc:/switch/ai1emu/logs/app.log. Each line is written
// and closed at once, so after a freeze the last line shows where the app stopped.
// appLogStart() replaces the previous run's log with a fresh one.
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#ifdef __SWITCH__
#include <SDL.h>
#endif
inline const char* appLogPath() { return "sdmc:/switch/ai1emu/logs/app.log"; }
inline std::mutex& appLogMutex() { static std::mutex m; return m; }
inline void appLogWrite(const char* mode, const std::string& line) {
#ifdef __SWITCH__
 std::lock_guard<std::mutex> lock(appLogMutex());
 if (FILE* f = std::fopen(appLogPath(), mode)) {
  std::fprintf(f, "[%7.2f s] %s\n", SDL_GetTicks64() / 1000.0, line.c_str());
  std::fclose(f);
 }
#else
 (void)mode; (void)line;
#endif
}
inline void appLog(const std::string& line) { appLogWrite("a", line); }
inline void appLogStart(const std::string& line) { appLogWrite("w", line); }
