import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handle} from '../src/worker.mjs';
const env={ASSETS:{fetch:async()=>new Response('test',{headers:{'Content-Type':'application/octet-stream'}})}};
test('manifest is never cached',async()=>{const r=await handle(new Request('https://test/latest.json'),env);assert.equal(r.headers.get('Cache-Control'),'no-store');});
test('release downloads stream and cache immutably',async()=>{const r=await handle(new Request('https://test/releases/0.2.2/AI1Emu.nro'),env);assert.equal(r.status,200);assert.match(r.headers.get('Cache-Control'),/immutable/);assert.equal(await r.text(),'test');});
test('read only and no arbitrary files',async()=>{assert.equal((await handle(new Request('https://test/latest.json',{method:'POST'}),env)).status,405);assert.equal((await handle(new Request('https://test/prod.keys'),env)).status,404);});
test('storage failure degrades cleanly',async()=>{assert.equal((await handle(new Request('https://test/latest.json'),{ASSETS:{fetch:async()=>{throw Error('offline');}}})).status,503);});
