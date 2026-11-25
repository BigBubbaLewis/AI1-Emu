#define SDL_MAIN_HANDLED
#include "online_cheats.h"
#include <cassert>
#include <iostream>
int main(){std::vector<CheatCandidate> out;const std::string id(24,'a'),sha(64,'b');const auto row=id+"\t120\t"+sha+"\t2\tPokemon Emerald (USA)";assert(parseCheatCatalog("AI1CHEATS1\n"+row,out)&&out.size()==1&&out[0].bytes==120);assert(parseCheatCatalog("AI1CHEATS1\n",out)&&out.empty());for(const auto& s:std::vector<std::string>{"<html>Error</html>","AI1CHEATS1\n../../escape\t120\t"+sha+"\t2\tGame","AI1CHEATS1\n"+id+"\t99999999\t"+sha+"\t2\tGame","AI1CHEATS1\n"+id+"\t120\t"+sha+"\t513\tGame"})assert(!parseCheatCatalog(s,out));std::cout<<"PASS: cheat selection protocol, empty results, invalid paths, oversized files and excessive cheat counts\n";}
