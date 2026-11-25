// SPDX-License-Identifier: MIT
#pragma once
#include <string>
// Performance boost for emulators AI1 Emu cannot clock directly (Dekopon, 3DS).
// sys-clk applies per-title clock profiles from sdmc:/config/sys-clk/config.ini, so AI1
// Emu writes a profile for its own title, marked as managed, and removes it again when
// the library opens. A section for the same title without the marker belongs to the
// owner (for example one saved from the sys-clk overlay) and is never changed.
// Clocks stay within Nintendo's own limits: CPU 1785 MHz, memory 1600 MHz.
enum class SysclkResult { Applied, Removed, Unchanged, OwnerProfile, Failed };
inline const char* sysclkMarker() { return "; managed by AI1 Emu: performance boost (removed when AI1 Emu opens)"; }
SysclkResult setSysclkBoost(const std::string& configPath, const std::string& titleId, bool on);
