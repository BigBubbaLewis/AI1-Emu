// v0.5 emulator update list (cores.txt) validation.
#include "update_manifest.h"
#include <cassert>
#include <iostream>
int main() {
 const std::string sha(64, 'a');
 CoreList list;
 std::string good = "version 0.5.0\r\nfceumm.nro 11585177 " + sha + " /releases/0.5.0/cores/fceumm.nro\nmupen64plus-next.nro 45680281 " + sha + " /releases/0.5.0/cores/mupen64plus-next.nro\n";
 assert(parseCoreList(good, list) && list.version == "0.5.0" && list.cores.size() == 2 && list.cores[1].size == 45680281);
 assert(parseCoreList(good + "dekopon.nro 37414956 " + sha + " /releases/0.5.0/cores/dekopon.nro\n", list) && list.cores.size() == 3); // 3DS
 assert(!parseCoreList("version 0.5.0\n", list));                                                          // no files
 assert(!parseCoreList("fceumm.nro 11585177 " + sha + " /releases/0.5.0/cores/fceumm.nro\n", list));        // version first
 assert(!parseCoreList("version 0.5.0\nhack.nro 11585177 " + sha + " /releases/0.5.0/cores/hack.nro\n", list)); // unknown file
 assert(!parseCoreList("version 0.5.0\nfceumm.nro 11585177 " + sha + " /releases/0.4.0/cores/fceumm.nro\n", list)); // wrong version path
 assert(!parseCoreList("version 0.5.0\nfceumm.nro 11585177 " + sha + " /../AI1Emu.nro\n", list));               // path escape
 assert(!parseCoreList("version 0.5.0\nfceumm.nro 11585177 ABC /releases/0.5.0/cores/fceumm.nro\n", list));      // bad hash
 assert(!parseCoreList("version 0.5.0\nfceumm.nro 99999999999 " + sha + " /releases/0.5.0/cores/fceumm.nro\n", list)); // too big
 assert(!parseCoreList(good + "fceumm.nro 11585177 " + sha + " /releases/0.5.0/cores/fceumm.nro\n", list));   // duplicate
 std::cout << "PASS: emulator update list accepts the published format and rejects unknown, oversized, duplicate, mis-versioned and escaping entries\n";
 return 0;
}
