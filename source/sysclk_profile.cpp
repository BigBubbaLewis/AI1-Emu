// SPDX-License-Identifier: MIT
#include "sysclk_profile.h"
#include "safe_file.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::string upper(std::string s) { for (char& c : s) c = char(std::toupper((unsigned char)c)); return s; }
std::string trim(const std::string& s) {
 size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
 return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
}

SysclkResult setSysclkBoost(const std::string& configPath, const std::string& titleId, bool on) {
 std::string text;
 { std::ifstream in(configPath, std::ios::binary); if (in) text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); }
 const std::string eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
 // Split into lines without their endings.
 std::vector<std::string> lines;
 for (size_t at = 0; at < text.size();) {
  size_t end = text.find('\n', at);
  std::string line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
  if (!line.empty() && line.back() == '\r') line.pop_back();
  lines.push_back(line);
  if (end == std::string::npos) break;
  at = end + 1;
 }
 // Find this title's section: [header] up to the next header.
 const std::string header = "[" + upper(titleId) + "]";
 size_t start = lines.size(), stop = lines.size();
 for (size_t i = 0; i < lines.size(); ++i) {
  std::string t = upper(trim(lines[i]));
  if (start == lines.size()) { if (t == header) start = i; }
  else if (!t.empty() && t[0] == '[') { stop = i; break; }
 }
 bool managed = false;
 if (start != lines.size())
  for (size_t i = start + 1; i < stop; ++i) if (trim(lines[i]) == sysclkMarker()) managed = true;
 if (start != lines.size() && !managed) return SysclkResult::OwnerProfile;

 if (start != lines.size()) {
  // Remove our section, and the blank line we put before it.
  size_t from = start;
  if (from > 0 && trim(lines[from - 1]).empty()) --from;
  lines.erase(lines.begin() + long(from), lines.begin() + long(stop));
 } else if (!on) {
  return SysclkResult::Unchanged;
 }
 if (on) {
  while (!lines.empty() && trim(lines.back()).empty()) lines.pop_back();
  if (!lines.empty()) lines.push_back("");
  lines.push_back("[" + upper(titleId) + "]");
  lines.push_back(sysclkMarker());
  for (const char* mode : {"handheld", "handheld_charging", "handheld_charging_official", "docked"}) {
   lines.push_back(std::string(mode) + "_cpu=1785");
   lines.push_back(std::string(mode) + "_mem=1600");
  }
 }
 std::string out;
 for (auto& l : lines) out += l + eol;
 if (!safeWriteFile(configPath, out.data(), out.size())) return SysclkResult::Failed;
 return on ? SysclkResult::Applied : SysclkResult::Removed;
}
