// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <functional>
#include <string>
struct MigrationResult {bool attempted=false,complete=false;size_t copied=0,duplicates=0,conflicts=0;uintmax_t freed=0;std::string message;};
// Only operates on switchDirectory/retroverse and switchDirectory/ai1emu.
// Conflicts retain the active AI1 file and preserve the legacy file separately.
MigrationResult mergeLegacyFolder(const std::filesystem::path& switchDirectory,
 const std::function<bool(const std::string&)>& progress={});
