// SPDX-License-Identifier: MIT
#include "network.h"
#include "app_log.h"
#include <cstdlib>
#include <mutex>
#ifdef __SWITCH__
#include <switch.h>
#include <curl/curl.h>
namespace {
void networkStop() { curl_global_cleanup(); socketExit(); }
}
#endif
bool networkStart() {
#ifdef __SWITCH__
 static std::mutex mutex;
 static bool ready = false;
 std::lock_guard<std::mutex> lock(mutex);
 if (ready) return true;
 appLog("Network: starting sockets");
 if (R_FAILED(socketInitializeDefault())) { appLog("Network: sockets failed"); return false; }
 appLog("Network: starting HTTPS");
 if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) { socketExit(); appLog("Network: HTTPS failed"); return false; }
 appLog("Network: ready");
 ready = true;
 std::atexit(networkStop); // after main() returns, once every download thread has been joined
 return true;
#else
 return false;
#endif
}
