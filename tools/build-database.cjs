// Derived database remains CC-BY-SA-4.0; see licenses/libretro-database-LICENSE.txt.
const fs=require('node:fs');
const data=fs.readFileSync('tools/gba-no-intro.dat','utf8');
const rows=[];
for(const m of data.matchAll(/^game \(\r?\n([\s\S]*?)^\)/gm)){
 const b=m[1], title=b.match(/^\s*name "([^"]+)"/m)?.[1];
 const crc=b.match(/\bcrc ([0-9A-F]+)/i)?.[1]||'',sha1=b.match(/\bsha1 ([0-9A-F]+)/i)?.[1]||'',serial=b.match(/\bserial "([^"]+)"/)?.[1]||'';
 if(title&&sha1)rows.push({title,crc,sha1,serial});
}
fs.writeFileSync('romfs/gba-database.tsv',rows.map(r=>[r.sha1,r.crc,r.serial,r.title].join('\t')).join('\n')+'\n');
fs.writeFileSync('tools/gba-database.json',JSON.stringify(rows));
console.log(`Generated ${rows.length} GBA release records`);
