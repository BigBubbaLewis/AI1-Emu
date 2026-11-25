// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// On-screen keyboard (Switch software keyboard applet). Returns false when the
// user cancels or the keyboard is unavailable (for example in the desktop preview).
#pragma once
#include <string>
#ifdef __SWITCH__
#include <switch.h>
inline bool showKeyboard(const std::string& guide, const std::string& initial, std::string& out, size_t maxLength = 64) {
 SwkbdConfig kbd;
 if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
 swkbdConfigMakePresetDefault(&kbd);
 swkbdConfigSetGuideText(&kbd, guide.c_str());
 swkbdConfigSetInitialText(&kbd, initial.c_str());
 swkbdConfigSetStringLenMax(&kbd, u32(maxLength));
 char buffer[256] = {};
 Result rc = swkbdShow(&kbd, buffer, sizeof buffer);
 swkbdClose(&kbd);
 if (R_FAILED(rc)) return false;
 out = buffer;
 return true;
}
#else
inline bool (*previewKeyboard)(const std::string&, const std::string&, std::string&) = nullptr;
inline bool showKeyboard(const std::string& guide, const std::string& initial, std::string& out, size_t = 64) {
 return previewKeyboard ? previewKeyboard(guide, initial, out) : false;
}
#endif
