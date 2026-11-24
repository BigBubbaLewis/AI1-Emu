import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root=path.resolve(import.meta.dirname,'../..'),pub=path.join(root,'cloudflare/public');
fs.mkdirSync(pub,{recursive:true});
if(!fs.existsSync(path.join(pub,'latest.json'))){
 const origin='https://ai1emu-updates.freegamez.workers.dev';
 const response=await fetch(origin+'/latest.json');if(!response.ok)throw Error('Cannot bootstrap existing stable manifest');const stable=await response.json();
 if(!/^\/releases\/\d+\.\d+\.\d+\/AI1Emu\.nro$/.test(stable.path))throw Error('Invalid stable path');
 for(const name of [stable.path,'/NOTICE.txt','/source/mgba-0.11-ai1emu.zip','/source/mGBA-MPL-2.0.txt','/source/SDL2-LICENSE.txt','/source/libnx-LICENSE.txt','/source/DejaVu-LICENSE.txt','/source/libretro-database-LICENSE.txt','/source/DATABASE-ATTRIBUTION.txt','/source/stb-LICENSE.txt']){const r=await fetch(origin+name);if(!r.ok)throw Error('Cannot retain stable asset '+name);const dest=path.join(pub,name);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,Buffer.from(await r.arrayBuffer()));}
 fs.writeFileSync(path.join(pub,'latest.json'),JSON.stringify(stable,null,2));
}
const version=fs.readFileSync(path.join(root,'source/app_version.h'),'utf8').match(/AI1EMU_VERSION "([0-9.]+)"/)[1];
const preview=process.argv.includes('--preview');
if(preview&&!fs.existsSync(path.join(pub,'latest-multicore.json')))throw Error('Preview publishing requires the existing stable release assets; use the established release workspace.');
const stableVersion=preview?JSON.parse(fs.readFileSync(path.join(pub,'latest-multicore.json'),'utf8')).version:version;
const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
const oldIndex=fs.existsSync(path.join(pub,'download-index.json'))?JSON.parse(fs.readFileSync(path.join(pub,'download-index.json'),'utf8')):{};
const downloadIndex={...oldIndex};
function archive(file,url,type){
 const data=fs.readFileSync(path.join(root,file)),sha=hash(data),parts=[];
 if(oldIndex[url]&&oldIndex[url].sha256!==sha)throw Error('An archive is already staged at this version with different bytes. Increase version before publishing.');
 for(let at=0,i=0;at<data.length;at+=22*1024*1024,i++){
  const name=`/_downloads/${sha}/part-${i}.bin`,dest=path.join(pub,name);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,data.subarray(at,at+22*1024*1024));parts.push(name);
 }
 downloadIndex[url]={parts,size:data.length,sha256:sha,type,filename:path.basename(file)};
}
archive(`build/AI1-Emu-v${version}-Corresponding-Source.tar.gz`,`/source/${version}/AI1-Emu-v${version}-Corresponding-Source.tar.gz`,'application/gzip');
archive(`AI1-Emu-v${version}-Switch-Package.zip`,`/releases/${version}/AI1-Emu-v${version}-Switch-Package.zip`,'application/zip');
const n64File='build/switch/cores/mupen64plus-next.nro',n64Path=`/releases/${version}/cores/mupen64plus-next.nro`;
archive(n64File,n64Path,'application/octet-stream');
const n64Bytes=fs.readFileSync(path.join(root,n64File));if(!preview)fs.writeFileSync(path.join(pub,'n64.json'),JSON.stringify({version,path:n64Path,size:n64Bytes.length,sha256:hash(n64Bytes)}));
const dsFile='build/switch/cores/melonds.nro',dsPath=`/releases/${version}/cores/melonds.nro`;
archive(dsFile,dsPath,'application/octet-stream');
const dsBytes=fs.readFileSync(path.join(root,dsFile));if(!preview)fs.writeFileSync(path.join(pub,'ds.json'),JSON.stringify({version,path:dsPath,size:dsBytes.length,sha256:hash(dsBytes)}));
fs.writeFileSync(path.join(pub,'download-index.json'),JSON.stringify(downloadIndex));
const catalog=[];
// Explicit source-only allowlist: no deployed assets, credentials, SDK binaries,
// build folders, user ROMs, BIOS, personal artwork or private upload sessions.
function source(file){const relative=path.relative(root,file).replaceAll('\\','/');if(/(^|\/)(?:\.env|prod\.keys|title\.keys|\.deploy-private|node_modules|\.wrangler)(\/|$)/i.test(relative))throw Error('Private file in source allowlist');
 const dest=path.join(pub,`source/${version}/files`,relative);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.copyFileSync(file,dest);catalog.push({path:relative,url:`/source/${version}/files/${relative}`,size:fs.statSync(file).size});}
function walk(dir){for(const item of fs.readdirSync(dir,{withFileTypes:true})){const file=path.join(dir,item.name);if(item.isDirectory())walk(file);else if(fs.statSync(file).size<=2*1024*1024&&/\.(?:cpp|h|c|py|mjs|cjs|json|jsonc|md|txt|sh|ps1|cmd|html)$/i.test(file))source(file);}}
for(const name of ['source','scripts','tests','docs','licenses','tools'])walk(path.join(root,name));
for(const name of ['Makefile','README.md','COLLABORATING.md','cloudflare/README.md','cloudflare/wrangler.jsonc','cloudflare/package.json','cloudflare/package-lock.json','cloudflare/tsconfig.json','cloudflare/worker-configuration.d.ts'])source(path.join(root,name));
for(const name of ['src','scripts','test','site'])walk(path.join(root,'cloudflare',name));
catalog.sort((a,b)=>a.path.localeCompare(b.path));fs.mkdirSync(path.join(pub,'developer'),{recursive:true});fs.writeFileSync(path.join(pub,'developer/catalog.json'),JSON.stringify({version,files:catalog}));
const nro=fs.readFileSync(path.join(root,'AI1Emu.nro'));if(nro.toString('ascii',16,20)!=='NRO0')throw Error('Invalid NRO');
const release=`/releases/${version}/AI1Emu.nro`;fs.mkdirSync(path.dirname(path.join(pub,release)),{recursive:true});if(fs.existsSync(path.join(pub,release))&&!fs.readFileSync(path.join(pub,release)).equals(nro))throw Error('Version already has a different NRO');fs.writeFileSync(path.join(pub,release),nro);
fs.writeFileSync(path.join(pub,preview?'preview.json':'latest-multicore.json'),JSON.stringify({schema:1,version,path:release,size:nro.length,sha256:hash(nro),source:`/source/${version}/AI1-Emu-v${version}-Corresponding-Source.tar.gz`},null,2));
for(const file of ['AI1Emu-Automatic-Covers-DBI.cmd','AI1Emu-Prepare-Artwork.cmd','Prepare-Artwork.cmd']){const dest=path.join(pub,`tools/${version}`,file);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.copyFileSync(path.join(root,'tools',file),dest);}
fs.copyFileSync(path.join(root,'romfs/app-icon.png'),path.join(pub,'logo.png'));
for(const [name,dest] of [['index.html','index.html'],['developer.html','developer/index.html'],['developer.mjs','developer/app.mjs']]){
 let body=fs.readFileSync(path.join(root,'cloudflare/site',name),'utf8').replaceAll('0.3.2',preview&&name==='index.html'?stableVersion:version);
 if(preview&&name==='index.html')body=body.replace('</html>',`<section><h2>v${version} UI preview</h2><p>Switch build and host tests passed. Hardware touch, frame pacing and memory remain unverified. The normal updater stays on v${stableVersion}.</p><a class="button" href="/releases/${version}/AI1-Emu-v${version}-Switch-Package.zip">Download preview package</a><p><a href="/source/${version}/AI1-Emu-v${version}-Corresponding-Source.tar.gz">Preview corresponding source</a></p></section></html>`);
 fs.writeFileSync(path.join(pub,dest),body);
}

const manifest={},files={};
function assets(dir){for(const item of fs.readdirSync(dir,{withFileTypes:true})){if(item.name.startsWith('.')||/^(?:prod|title)\.keys$/i.test(item.name))throw Error('Private path in public assets');const file=path.join(dir,item.name);if(item.isDirectory()){assets(file);continue;}const data=fs.readFileSync(file);if(data.length>25*1024*1024)throw Error('Asset too large');const name='/'+path.relative(pub,file).replaceAll('\\','/');const key=hash(data.toString('base64')+path.extname(file).slice(1)).slice(0,32);manifest[name]={hash:key,size:data.length};files[key]={path:file,type:file.endsWith('.json')?'application/json':file.endsWith('.html')?'text/html; charset=utf-8':file.endsWith('.mjs')?'text/javascript; charset=utf-8':file.endsWith('.txt')?'text/plain; charset=utf-8':file.endsWith('.png')?'image/png':'application/octet-stream'};}}
assets(pub);const privateDir=path.join(root,'cloudflare/.deploy-private');fs.mkdirSync(privateDir,{recursive:true});fs.writeFileSync(path.join(privateDir,'manifest.json'),JSON.stringify(manifest));fs.writeFileSync(path.join(privateDir,'files.json'),JSON.stringify(files));
console.log(`Prepared ${version}: ${catalog.length} browsable source files, complete source/package streams, ${Object.keys(manifest).length} assets. Stable GBA manifest retained. ${preview?"Modern/component channels retained; preview only.":"Modern release channel updated."}`);
