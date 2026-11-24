// PC-only preparation. RetroVerse itself never opens a network connection.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),https=require('node:https'),{spawnSync}=require('node:child_process');
const db=JSON.parse(fs.readFileSync(path.join(__dirname,'gba-database.json'),'utf8'));
const normalized=s=>s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/\([^)]*\)|\[[^\]]*\]/g,'').toLowerCase().replace(/[^a-z0-9]/g,'');
const artName=s=>s.replace(/[&*/:`<>?\\|]/g,'_');
function identify(filename,bytes){
 if(bytes){const sha1=crypto.createHash('sha1').update(bytes).digest('hex').toUpperCase();const exact=db.find(r=>r.sha1===sha1);if(exact)return {...exact,method:'SHA-1'};}
 const stem=path.basename(filename).replace(/\.gba$/i,'');const exact=db.find(r=>r.title===stem);if(exact)return {...exact,method:'filename'};
 if(bytes&&bytes.length>=192){const serial=bytes.subarray(0xac,0xb0).toString('ascii');const candidates=db.filter(r=>r.serial===serial);if(candidates.length){const revision=bytes[0xbc];const found=candidates.find(r=>revision? r.title.includes(`(Rev ${revision})`):!r.title.includes('(Rev '))||candidates[0];return {...found,method:'header serial (not checksum verified)'};}}
 const candidates=db.filter(r=>normalized(r.title)===normalized(stem));
 if(candidates.length){const region=stem.match(/\((USA|Europe|Japan)[^)]*\)/i)?.[1];return {...(candidates.find(r=>region&&r.title.includes('('+region))||candidates.find(r=>r.title.includes('(USA')&&!r.title.includes('(Rev '))||candidates[0]),method:'normalized filename (not checksum verified)'};}
 return null;
}
function get(url){return new Promise((resolve,reject)=>{const req=https.get(url,{headers:{'User-Agent':'RetroVerse-PC-Artwork-Prep/0.2'}},res=>{if(res.statusCode>=300&&res.statusCode<400&&res.headers.location){res.resume();get(new URL(res.headers.location,url).href).then(resolve,reject);return;}if(res.statusCode!==200){res.resume();reject(new Error('HTTP '+res.statusCode));return;}let size=0,chunks=[];res.on('data',b=>{size+=b.length;if(size>8*1024*1024){req.destroy(new Error('Response too large'));return;}chunks.push(b);});res.on('end',()=>resolve(Buffer.concat(chunks)));res.on('error',reject);});req.setTimeout(20000,()=>req.destroy(new Error('Connection timed out')));req.on('error',reject);});}
const popular=['Pokemon - Emerald Version (USA, Europe)','Pokemon - FireRed Version (USA)','Pokemon - Ruby Version (USA)','Pokemon - Sapphire Version (USA)','Kirby - Nightmare in Dream Land (USA)','Metroid Fusion (USA)','Legend of Zelda, The - The Minish Cap (USA)','Mario & Luigi - Superstar Saga (USA)','Advance Wars (USA)'];
async function prepare(root,{offline=false,curated=false}={}){
 root=path.resolve(root);const cache=path.join(root,'artwork-cache');fs.mkdirSync(cache,{recursive:true});
 const report=[];let jobs=[];
 if(curated)jobs=popular.map(name=>({filename:name+'.gba',match:identify(name+'.gba')}));
 else{for(const folder of ['roms','covers','metadata'])fs.mkdirSync(path.join(root,folder),{recursive:true});jobs=fs.readdirSync(path.join(root,'roms')).filter(f=>/\.gba$/i.test(f)).map(filename=>({filename,match:identify(filename,fs.readFileSync(path.join(root,'roms',filename)))}));}
 for(const job of jobs){const {filename,match}=job;if(!match){report.push({filename,result:'Unrecognized; custom cover fallback available'});continue;}
 const name=filename.replace(/\.gba$/i,''),key=artName(match.title),cached=path.join(cache,key+'.png');
 const custom=!curated&&['.png','.jpg','.jpeg'].some(ext=>fs.existsSync(path.join(root,'covers',name+ext)));
 let result=custom?'Custom cover preserved':fs.existsSync(cached)?'Cached':'Missing';
 if(!custom&&!fs.existsSync(cached)&&!offline){
  const url='https://raw.githubusercontent.com/libretro-thumbnails/Nintendo_-_Game_Boy_Advance/master/Named_Boxarts/'+encodeURIComponent(key+'.png');
  const temp=cached+'.download',optimized=cached+'.tmp.png';
  try{const image=await get(url);fs.writeFileSync(temp,image);const run=spawnSync('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(__dirname,'optimize-art.ps1'),'-InputPath',temp,'-OutputPath',optimized],{encoding:'utf8',windowsHide:true});if(run.status!==0)throw new Error(run.stderr||'Image optimization failed');fs.renameSync(optimized,cached);result='Downloaded and cached';fs.writeFileSync(cached+'.source.txt',url+'\nPersonal cache; not cleared for redistribution.\n');}catch(e){result='Unavailable: '+e.message;}finally{for(const tmp of [temp,optimized])if(fs.existsSync(tmp))fs.unlinkSync(tmp);}
 }
 if(!curated){const meta=path.join(root,'metadata',name+'.txt');if(!fs.existsSync(meta))fs.writeFileSync(meta,`title=${match.title}\nidentification=${match.method}\n`);}
 report.push({filename,title:match.title,method:match.method,result});console.log(filename+': '+result);
 }
 fs.writeFileSync(path.join(root,'artwork-report.json'),JSON.stringify(report,null,2));return report;
}
if(require.main===module){const args=process.argv.slice(2);const root=args.find(a=>!a.startsWith('--'));if(!root){console.error('Usage: node tools/prepare-artwork.cjs <SD:/switch/retroverse> [--offline] [--curated]');process.exitCode=1;}else prepare(root,{offline:args.includes('--offline'),curated:args.includes('--curated')}).catch(e=>{console.error(e.message);process.exitCode=1;});}
module.exports={identify,normalized,artName,prepare,popular,db};


