import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handle} from '../src/worker.mjs';
const env={ASSETS:{fetch:async()=>new Response('test',{headers:{'Content-Type':'application/octet-stream'}})}};
test('manifest is never cached',async()=>{const r=await handle(new Request('https://test/latest.json'),env);assert.equal(r.headers.get('Cache-Control'),'no-store');});
test('release downloads stream and cache immutably',async()=>{const r=await handle(new Request('https://test/releases/0.2.2/AI1Emu.nro'),env);assert.equal(r.status,200);assert.match(r.headers.get('Cache-Control'),/immutable/);assert.equal(await r.text(),'test');});
test('read only and no arbitrary files',async()=>{assert.equal((await handle(new Request('https://test/latest.json',{method:'POST'}),env)).status,405);assert.equal((await handle(new Request('https://test/prod.keys'),env)).status,404);});
test('emulator list is served uncached',async()=>{const r=await handle(new Request('https://test/cores.txt'),env);assert.equal(r.status,200);assert.equal(r.headers.get('Cache-Control'),'no-store');});
test('every emulator file uses the verified streaming index',async()=>{
 const sha='a'.repeat(64);const parts=['/_downloads/'+sha+'/part-0.bin'];
 const index={};for(const name of ['fceumm','snes9x','genesis-plus-gx','melonds','mupen64plus-next','dekopon'])index[`/releases/0.5.0/cores/${name}.nro`]={parts,size:4,sha256:sha,type:'application/octet-stream',filename:name+'.nro'};
 const body=JSON.stringify(index);
 const streaming={ASSETS:{fetch:async(/** @type {Request} */ req)=>new URL(req.url).pathname==='/download-index.json'?new Response(body,{headers:{'Content-Length':String(body.length)}}):new Response('data')}};
 for(const name of ['fceumm','snes9x','genesis-plus-gx','melonds','mupen64plus-next','dekopon']){const r=await handle(new Request(`https://test/releases/0.5.0/cores/${name}.nro`),streaming);assert.equal(r.status,200);assert.equal(r.headers.get('ETag'),`"${sha}"`);assert.equal(await r.text(),'data');}
 assert.equal((await handle(new Request('https://test/releases/0.5.0/cores/other.nro'),streaming)).status,404);
});
test('storage failure degrades cleanly',async()=>{assert.equal((await handle(new Request('https://test/latest.json'),{ASSETS:{fetch:async()=>{throw Error('offline');}}})).status,503);});
