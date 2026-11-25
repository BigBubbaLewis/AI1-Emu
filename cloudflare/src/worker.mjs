/** @param {Request} request @param {Env} env @param {ExecutionContext} [ctx] */
export async function handle(request,env,ctx){
 const url=new URL(request.url);
 let decoded;try{decoded=decodeURIComponent(url.pathname);}catch{return new Response('Not found',{status:404});}
 if(decoded.split('/').some(p=>p.startsWith('.')||/^(?:prod|title)\.keys$/i.test(p)))return new Response('Not found',{status:404});
 if(!['GET','HEAD'].includes(request.method))return new Response('Method not allowed',{status:405,headers:{Allow:'GET, HEAD'}});
 if(url.pathname==='/health')return Response.json({service:'AI1 Emu updates',status:'ok'});
 const component=/^\/releases\/\d+\.\d+\.\d+\/cores\/(?:mupen64plus-next|melonds|fceumm|snes9x|genesis-plus-gx|dekopon)\.nro$/.test(url.pathname);
 const download=component||/^\/(?:source|releases)\/\d+\.\d+\.\d+\/[A-Za-z0-9._-]+\.(?:zip|tar\.gz)$/.test(url.pathname);
 const source=/^\/source\/\d+\.\d+\.\d+\/files\/[A-Za-z0-9_./+-]+$/.test(url.pathname);
 const page=['/','/developer/','/developer/index.html','/developer/catalog.json','/developer/app.mjs','/logo.png','/latest-multicore.json','/n64.json','/ds.json','/preview.json','/cores.txt'].includes(url.pathname);
 const tool=/^\/tools\/\d+\.\d+\.\d+\/[A-Za-z0-9._-]+\.(?:cmd|txt)$/.test(url.pathname);
 if(!page&&!source&&!download&&!tool&&url.pathname!=='/latest.json'&&url.pathname!=='/NOTICE.txt'&&!/^\/releases\/\d+\.\d+\.\d+\/AI1Emu\.nro$/.test(url.pathname)&&!/^\/source\/mgba-[a-z0-9.-]+\.zip$/.test(url.pathname)&&!/^\/source\/[A-Za-z0-9._-]+\.txt$/.test(url.pathname))return new Response('Not found',{status:404});
 try{
  if(download)return await archiveDownload(request,env,ctx);
  let assetRequest=request;
  if(url.pathname==='/latest.json'){
   const version=request.headers.get('User-Agent')?.match(/^AI1Emu\/(\d+)\.(\d+)\.(\d+)$/);
   // Only clients which already have the v0.3.1 companions receive main-only
   // multi-console updates. Older installations retain the stable GBA channel.
   if(version&&(Number(version[1])>0||Number(version[2])>3||(Number(version[2])===3&&Number(version[3])>=1))){const target=new URL(request.url);target.pathname='/latest-multicore.json';assetRequest=new Request(target,request);}
  }
  const asset=await env.ASSETS.fetch(assetRequest);
  const response=new Response(asset.body,asset);
  response.headers.set('X-Content-Type-Options','nosniff');
  response.headers.set('Cache-Control',url.pathname.includes('latest')||url.pathname==='/NOTICE.txt'||page?'no-store':'public, max-age=31536000, immutable');
  if(url.pathname==='/latest.json')response.headers.set('Vary','User-Agent');
  if(source)response.headers.set('Content-Type','text/plain; charset=utf-8');
  if(tool)response.headers.set('Content-Disposition','attachment; filename="'+url.pathname.split('/').pop()+'"');
  return response;
 }catch{return new Response('Release unavailable',{status:503});}
}
/** @param {Request} request @param {Env} env @param {ExecutionContext} [ctx] */
async function archiveDownload(request,env,ctx){
 const url=new URL(request.url),indexUrl=new URL('/download-index.json',url);
 const metadata=await env.ASSETS.fetch(new Request(indexUrl));
 if(!metadata.ok||Number(metadata.headers.get('Content-Length'))>65536)return new Response('Download unavailable',{status:503});
 // Owned static metadata; still cap the read so a bad deployment cannot buffer
 // an unbounded response in a Worker isolate.
 const input=metadata.body?.getReader();if(!input)return new Response('Download unavailable',{status:503});
 const chunks=[];let bytes=0;
 try{while(true){const part=await input.read();if(part.done)break;bytes+=part.value.length;if(bytes>65536){await input.cancel();return new Response('Download unavailable',{status:503});}chunks.push(part.value);}}finally{input.releaseLock();}
 const raw=new Uint8Array(bytes);let offset=0;for(const chunk of chunks){raw.set(chunk,offset);offset+=chunk.length;}
 const index=JSON.parse(new TextDecoder().decode(raw));const entry=index[url.pathname];
 if(!entry)return new Response('Not found',{status:404});
 if(!Number.isSafeInteger(entry.size)||entry.size<1||!Array.isArray(entry.parts)||entry.parts.length<1||entry.parts.length>16||!entry.parts.every((/** @type {unknown} */ p)=>typeof p==='string'&&/^\/_downloads\/[a-f0-9]{64}\/part-\d+\.bin$/.test(p)))return new Response('Download unavailable',{status:503});
 const headers={'Content-Type':entry.type,'Content-Length':String(entry.size),'Content-Disposition':'attachment; filename="'+entry.filename+'"','Cache-Control':'public, max-age=31536000, immutable','X-Content-Type-Options':'nosniff','ETag':'"'+entry.sha256+'"'};
 if(request.method==='HEAD')return new Response(null,{headers});
 // Native pipeTo keeps large archives out of JS memory and avoids a JS callback
 // for every chunk. FixedLengthStream also enforces the advertised byte count.
 const stream=typeof FixedLengthStream==='undefined'?new TransformStream():new FixedLengthStream(entry.size);
 const job=(async()=>{for(const part of entry.parts){const r=await env.ASSETS.fetch(new Request(new URL(part,url)));if(!r.ok||!r.body)throw Error('Missing archive part');await r.body.pipeTo(stream.writable,{preventClose:true});}const writer=stream.writable.getWriter();try{await writer.close();}finally{writer.releaseLock();}})().catch(async error=>{console.error('archive_stream_failed',{path:url.pathname,message:String(error)});try{await stream.writable.abort(error);}catch{}});
 if(ctx)ctx.waitUntil(job);else void job;
 return new Response(stream.readable,{headers});
}
export default {fetch:handle};

