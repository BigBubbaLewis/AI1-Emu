const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {identify,prepare,popular,db}=require('./prepare-artwork.cjs');
(async()=>{
 for(const name of popular){const match=identify(name+'.gba');assert(match,name);assert.equal(require("./prepare-artwork.cjs").normalized(match.title),require("./prepare-artwork.cjs").normalized(name));}
 assert.match(identify('Pokémon - Emerald Version.gba').title,/Emerald/);
 assert.match(identify('Pokemon - FireRed Version (Europe).gba').title,/FireRed/);
 const bytes=Buffer.alloc(192);bytes.write('BPEE',0xac,'ascii');assert.match(identify('renamed.gba',bytes).title,/Emerald/);
 bytes.write('BPEJ',0xac,'ascii');assert.match(identify('renamed.gba',bytes).title,/Japan/);
 const fixture=Buffer.from('RetroVerse checksum identification test');const entry={sha1:crypto.createHash('sha1').update(fixture).digest('hex').toUpperCase(),title:'Checksum fixture'};db.push(entry);assert.equal(identify('unknown.gba',fixture).method,'SHA-1');db.pop();
 assert.equal(identify('Unknown Homebrew Game.gba'),null);
 const root=path.resolve('build/artwork-test');fs.mkdirSync(path.join(root,'roms'),{recursive:true});fs.mkdirSync(path.join(root,'covers'),{recursive:true});
 fs.writeFileSync(path.join(root,'roms',popular[0]+'.gba'),Buffer.alloc(192));fs.writeFileSync(path.join(root,'covers',popular[0]+'.png'),'CUSTOM-DO-NOT-OVERWRITE');
 const report=await prepare(root,{offline:true});assert.equal(report[0].result,'Custom cover preserved');assert.equal(fs.readFileSync(path.join(root,'covers',popular[0]+'.png'),'utf8'),'CUSTOM-DO-NOT-OVERWRITE');
 fs.writeFileSync(path.join(root,'roms','Unknown Homebrew Game.gba'),Buffer.alloc(192));const again=await prepare(root,{offline:true});assert(again.some(r=>r.result.startsWith('Unrecognized')));
 console.log('PASS: nine popular titles, accented filename, regions, renamed header, SHA-1, unknown ROM, offline and custom preservation.');
})().catch(e=>{console.error(e);process.exitCode=1;});

