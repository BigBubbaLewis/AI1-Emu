#pragma once
#include <string>
#include <vector>
std::string gameTitle(const std::string& path);
bool gameRecognized(const std::string& path);
// Original release a patched game (ROM hack) is based on, or "" if it is not a known patch.
std::string gamePatchedFrom(const std::string& path);
// Loose name used to match your own covers to games despite tags, case, accents and versions.
std::string looseNameKey(const std::string& name);
std::vector<std::string> looseNameWords(const std::string& name);
// Best-fitting cover name for a game (its file name and title), or -1. See game_database.cpp.
int matchCover(const std::vector<std::string>& gameNames, const std::vector<std::string>& covers);
