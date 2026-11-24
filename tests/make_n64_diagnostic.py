# SPDX-License-Identifier: MIT
# Creates only original diagnostic MIPS code; no Nintendo boot ROM or game data.
from pathlib import Path
import struct
code=[]
def emit(x): code.append(x)
def load(reg,value): emit(0x3c000000|(reg<<16)|(value>>16));emit(0x34000000|(reg<<21)|(reg<<16)|(value&65535))
def store(base,offset,value): load(9,value);emit(0xac000000|(base<<21)|(9<<16)|offset)
load(8,0xa0100000);load(9,0xf801f801);load(10,0xa0125800)
loop=len(code);emit(0xad090000);emit(0x25080004);emit(0x150a0000|((loop-(len(code)+1))&65535));emit(0)
load(8,0xa4400000)
for offset,value in [(0,0x320e),(4,0x100000),(8,320),(12,2),(24,0x20d),(28,0xc15),(32,0xc150c15),(36,0x006c02ec),(40,0x002501ff),(44,0xe0204),(48,0x200),(52,0x400)]:store(8,offset,value)
load(8,0xa0001000);store(8,0,0x41493121)
load(8,0xa4500000)
for offset,value in [(0,0x200000),(8,1),(16,1101),(20,15),(4,4096)]:store(8,offset,value)
# Remain in DMEM; timer/VI events continue to run.
label=0xa4000040+len(code)*4;emit(0x08000000|((label>>2)&0x3ffffff));emit(0)
rom=bytearray(4*1024*1024);struct.pack_into('>IIII',rom,0,0x80371240,15,0x80000400,0);rom[0x20:0x34]=b'AI1 ORIGINAL N64 TEST';rom[0x3e]=0x45
for i,x in enumerate(code):struct.pack_into('>I',rom,0x40+i*4,x)
Path('build/n64-diagnostic.z64').write_bytes(rom)
print('Generated original N64 diagnostic in build only')
