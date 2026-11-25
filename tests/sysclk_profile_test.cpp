// sys-clk profile for the 3DS performance boost: added and removed by AI1 Emu, the
// owner's other profiles and settings untouched, the owner's own profile never changed.
#include "sysclk_profile.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
namespace fs = std::filesystem;
static std::string readAll(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
static void writeAll(const std::string& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }
int main() {
 const std::string dir = "build/sysclk-fixture/", cfg = dir + "config.ini", id = "018d742347c42000";
 fs::remove_all(dir); fs::create_directories(dir);
 // Nothing to remove when there is no profile.
 writeAll(cfg, "[values]\r\ntemp_log_interval_ms=0\r\n\r\n[0100F2C0115B6000]\r\nhandheld_cpu=1020\r\n");
 const std::string original = readAll(cfg);
 assert(setSysclkBoost(cfg, id, false) == SysclkResult::Unchanged && readAll(cfg) == original);
 // Applied: other sections kept as they were, CRLF kept, all four modes boosted.
 assert(setSysclkBoost(cfg, id, true) == SysclkResult::Applied);
 std::string boosted = readAll(cfg);
 assert(boosted.rfind(original, 0) == 0);
 assert(boosted.find("[018D742347C42000]\r\n") != std::string::npos);
 assert(boosted.find(sysclkMarker()) != std::string::npos);
 for (const char* key : {"handheld_cpu=1785", "handheld_mem=1600", "handheld_charging_cpu=1785", "handheld_charging_official_mem=1600", "docked_cpu=1785", "docked_mem=1600"}) assert(boosted.find(key) != std::string::npos);
 assert(boosted.find("\n[", boosted.find("[018D")) == std::string::npos); // ours is last; nothing duplicated
 // Applying again does not duplicate it.
 assert(setSysclkBoost(cfg, id, true) == SysclkResult::Applied && readAll(cfg) == boosted);
 // Removed: the file is back to exactly what the owner had.
 assert(setSysclkBoost(cfg, id, false) == SysclkResult::Removed && readAll(cfg) == original);
 // The owner's own profile for this title (e.g. saved from the sys-clk overlay) is left alone.
 const std::string owner = original + "\r\n[018D742347C42000]\r\nhandheld_cpu=1785\r\n";
 writeAll(cfg, owner);
 assert(setSysclkBoost(cfg, id, true) == SysclkResult::OwnerProfile && readAll(cfg) == owner);
 assert(setSysclkBoost(cfg, id, false) == SysclkResult::OwnerProfile && readAll(cfg) == owner);
 // A missing config file is created when the boost is applied.
 fs::remove(cfg);
 assert(setSysclkBoost(cfg, id, false) == SysclkResult::Unchanged && !fs::exists(cfg));
 assert(setSysclkBoost(cfg, id, true) == SysclkResult::Applied && readAll(cfg).rfind("[018D742347C42000]", 0) == 0);
 fs::remove_all(dir);
 std::cout << "PASS: sys-clk boost profile added and removed cleanly, other profiles untouched, owner's own profile kept\n";
 return 0;
}
