import {test} from 'node:test';import assert from 'node:assert/strict';import {handle} from '../src/worker.mjs';
test('modern installations get main-only multicore updates; old clients retain GBA channel',async()=>{
 const env={ASSETS:{fetch:async r=>new Response(new URL(r.url).pathname)}};
 for(const [ua,path] of [['AI1Emu/0.3.1','/latest-multicore.json'],['AI1Emu/0.3.2','/latest-multicore.json'],['AI1Emu/0.2.2','/latest.json'],['AI1Emu/0.3.0','/latest.json'],['browser','/latest.json']]){const r=await handle(new Request('https://test/latest.json',{headers:{'User-Agent':ua}}),env);assert.equal(await r.text(),path);assert.equal(r.headers.get('Cache-Control'),'no-store');assert.equal(r.headers.get('Vary'),'User-Agent');}
});
test('large downloads concatenate streamed assets and support metadata-only HEAD',async()=>{
 const part='/_downloads/'+ 'a'.repeat(64)+'/part-',name='/source/0.3.2/AI1-Emu-v0.3.2-Corresponding-Source.tar.gz';let reads=0;
 const env={ASSETS:{fetch:async r=>{const p=new URL(r.url).pathname;if(p==='/download-index.json')return Response.json({[name]:{parts:[part+'0.bin',part+'1.bin'],size:6,filename:'source.tar.gz',sha256:'b'.repeat(64),type:'application/gzip'}});reads++;return new Response(p.endsWith('0.bin')?'abc':'def');}}};
 const head=await handle(new Request('https://test'+name,{method:'HEAD'}),env);assert.equal(head.headers.get('Content-Length'),'6');assert.equal(reads,0);assert.equal(await head.text(),'');
 const r=await handle(new Request('https://test'+name),env);assert.equal(await r.text(),'abcdef');assert.equal(reads,2);assert.match(r.headers.get('Content-Disposition'),/source.tar.gz/);
});
test('hidden paths and keys are blocked even underneath the public source route',async()=>{
 let called=false;const env={ASSETS:{fetch:async()=>{called=true;return new Response('should not read');}}};
 for(const path of ['/source/0.3.2/files/.deploy-private/session.json','/source/0.3.2/files/prod.keys','/source/0.3.2/files/%2eenv'])assert.equal((await handle(new Request('https://test'+path),env)).status,404);
 assert.equal(called,false);
});
test('source pages are plain text while source catalogue is refreshed',async()=>{const env={ASSETS:{fetch:async()=>new Response('source')}};const r=await handle(new Request('https://test/source/0.3.2/files/source/main.cpp'),env);assert.match(r.headers.get('Content-Type'),/^text\/plain/);assert.equal((await handle(new Request('https://test/developer/catalog.json'),env)).headers.get('Cache-Control'),'no-store');});
test('N64 component streams only its fixed public path and its manifest stays fresh',async()=>{
 const name='/releases/0.3.3/cores/mupen64plus-next.nro',part='/_downloads/'+ 'a'.repeat(64)+'/part-0.bin';
 const env={ASSETS:{fetch:async r=>new URL(r.url).pathname==='/download-index.json'?Response.json({[name]:{parts:[part],size:4,filename:'mupen64plus-next.nro',sha256:'b'.repeat(64),type:'application/octet-stream'}}):new Response('NRO0')}};
 const r=await handle(new Request('https://test'+name),env);assert.equal(r.status,200);assert.equal(await r.text(),'NRO0');assert.equal((await handle(new Request('https://test/n64.json'),env)).headers.get('Cache-Control'),'no-store');assert.equal((await handle(new Request('https://test/releases/0.3.3/cores/unknown.nro'),env)).status,404);
});
test('DS audio component uses its fixed path and a fresh manifest',async()=>{
 const name='/releases/0.3.5/cores/melonds.nro',part='/_downloads/'+'c'.repeat(64)+'/part-0.bin';
 const env={ASSETS:{fetch:async r=>new URL(r.url).pathname==='/download-index.json'?Response.json({[name]:{parts:[part],size:4,filename:'melonds.nro',sha256:'d'.repeat(64),type:'application/octet-stream'}}):new Response('NRO0')}};
 assert.equal(await (await handle(new Request('https://test'+name),env)).text(),'NRO0');assert.equal((await handle(new Request('https://test/ds.json'),env)).headers.get('Cache-Control'),'no-store');assert.equal((await handle(new Request('https://test/releases/0.3.5/cores/other.nro'),env)).status,404);
});
test('UI preview manifest is fresh and does not replace normal updater routing',async()=>{const env={ASSETS:{fetch:async r=>new Response(new URL(r.url).pathname)}};const p=await handle(new Request('https://test/preview.json'),env);assert.equal(await p.text(),'/preview.json');assert.equal(p.headers.get('Cache-Control'),'no-store');const stable=await handle(new Request('https://test/latest.json',{headers:{'User-Agent':'AI1Emu/0.3.5'}}),env);assert.equal(await stable.text(),'/latest-multicore.json');});
