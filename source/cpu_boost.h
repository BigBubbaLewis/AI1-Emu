// SPDX-License-Identifier: MIT - AI1 Emu contributors, 2026.
// Optional "Performance boost": raises the CPU clock while a game runs and puts the
// original clock back when it is turned off or the game closes. Off by default.
// Uses clkrst (firmware 8.0+) or pcv (older firmware), the same services sys-clk uses.
#pragma once
#include <switch.h>
#include <SDL.h>

class CpuBoost {
public:
 static constexpr u32 boostHz = 1785000000u; // highest CPU clock that is stable on every model
 ~CpuBoost() { set(false); }
 void set(bool on) {
  if (on == active) return;
  if (on) {
   if (!open()) return;
   original = read();
   if (!original) original = 1020000000u;
   active = apply(boostHz);
   if (!active) close();
   lastApply = SDL_GetTicks64();
  } else {
   apply(original ? original : 1020000000u);
   close();
   active = false;
  }
 }
 // The system can reset clocks (for example after the HOME menu or docking), so
 // re-apply every few seconds while the boost is on.
 void keep() {
  if (!active) return;
  Uint64 now = SDL_GetTicks64();
  if (now - lastApply < 3000) return;
  lastApply = now;
  if (read() != boostHz) apply(boostHz);
 }
 bool on() const { return active; }

private:
 bool active = false, useClkrst = false, opened = false;
 u32 original = 0;
 Uint64 lastApply = 0;
 ClkrstSession session{};
 bool open() {
  if (opened) return true;
  useClkrst = hosversionAtLeast(8, 0, 0);
  if (useClkrst) {
   if (R_FAILED(clkrstInitialize())) return false;
   if (R_FAILED(clkrstOpenSession(&session, PcvModuleId_CpuBus, 3))) { clkrstExit(); return false; }
  } else if (R_FAILED(pcvInitialize())) return false;
  opened = true;
  return true;
 }
 void close() {
  if (!opened) return;
  if (useClkrst) { clkrstCloseSession(&session); clkrstExit(); } else pcvExit();
  opened = false;
 }
 u32 read() {
  u32 hz = 0;
  if (!opened) return 0;
  if (useClkrst) { if (R_FAILED(clkrstGetClockRate(&session, &hz))) hz = 0; }
  else if (R_FAILED(pcvGetClockRate(PcvModule_CpuBus, &hz))) hz = 0;
  return hz;
 }
 bool apply(u32 hz) {
  if (!opened) return false;
  return useClkrst ? R_SUCCEEDED(clkrstSetClockRate(&session, hz)) : R_SUCCEEDED(pcvSetClockRate(PcvModule_CpuBus, hz));
 }
};
