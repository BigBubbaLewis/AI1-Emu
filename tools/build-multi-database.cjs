const fs=require('node:fs');
const systems={n64:'Nintendo_-_Nintendo_64',gba:'Nintendo_-_Game_Boy_Advance',gb:'Nintendo_-_Game_Boy',gbc:'Nintendo_-_Game_Boy_Color',nes:'Nintendo_-_Nintendo_Entertainment_System',snes:'Nintendo_-_Super_Nintendo_Entertainment_System',genesis:'Sega_-_Mega_Drive_-_Genesis',sms:'Sega_-_Master_System_-_Mark_III',gg:'Sega_-_Game_Gear',nds:'Nintendo_-_Nintendo_DS',dsi:'Nintendo_-_Nintendo_DSi'};
const thumbnails=JSON.parse(fs.readFileSync("tools/databases/thumbnail-names.json","utf8"));
const output=[];
for(const [slug,repository] of Object.entries(systems)){
 const source=`tools/databases/${slug}.dat`;if(!fs.existsSync(source))continue;
 const text=fs.readFileSync(source,'utf8'),games=[];
 for(const m of text.matchAll(/^game \(\r?\n([\s\S]*?)^\)/gm)){const block=m[1],title=block.match(/^\s*name "([^"]+)"/m)?.[1];if(title)games.push({title,sha1:block.match(/\bsha1 ([0-9A-F]+)/i)?.[1]||'',crc:block.match(/\bcrc ([0-9A-F]+)/i)?.[1]||'',serial:block.match(/\bserial "([^"]+)"/)?.[1]||''});}
 fs.writeFileSync(`romfs/${slug}-database.tsv`,games.map(g=>[g.sha1,g.crc,g.serial,g.title].join('\t')).join('\n')+'\n');
 output.push({slug,repository,games,boxarts:thumbnails[slug]||[]});console.log(`${slug}: ${games.length} release records`);
}
fs.writeFileSync('tools/databases/multi-database.json',JSON.stringify(output));
