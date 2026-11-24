import fs from 'node:fs';
import path from 'node:path';
import https from 'node:https';
const privateDir=path.resolve(import.meta.dirname,'../.deploy-private');
const session=JSON.parse(fs.readFileSync(path.join(privateDir,'session.json'),'utf8'));
const files=JSON.parse(fs.readFileSync(path.join(privateDir,'files.json'),'utf8'));
let jwt=session.jwt;
for(const bucket of session.buckets){
 const boundary='AI1AssetBoundary'+Date.now();
 const parts=[];
 for(const hash of bucket){const file=files[hash];parts.push(Buffer.from(`--${boundary}\r\nContent-Disposition: form-data; name="${hash}"\r\nContent-Type: ${file.type}\r\n\r\n${fs.readFileSync(file.path).toString('base64')}\r\n`));}
 parts.push(Buffer.from(`--${boundary}--\r\n`));const body=Buffer.concat(parts);
 const result=await new Promise((resolve,reject)=>{const req=https.request(`https://api.cloudflare.com/client/v4/accounts/${session.accountId}/workers/assets/upload?base64=true`,{method:'POST',headers:{Authorization:`Bearer ${session.jwt}`,'Content-Type':`multipart/form-data; boundary=${boundary}`,'Content-Length':body.length}},res=>{let data='';res.on('data',d=>data+=d);res.on('end',()=>{try{const r=JSON.parse(data);if(!r.success)reject(Error(JSON.stringify(r.errors)));else resolve(r.result);}catch(e){reject(e);}});});req.setTimeout(180000,()=>req.destroy(Error('Upload timed out')));req.on('error',reject);req.end(body);});
 if(result.jwt)jwt=result.jwt;
 console.log(`Uploaded ${bucket.length} release assets`);
}
fs.writeFileSync(path.join(privateDir,'completion.json'),JSON.stringify({jwt}));
console.log('Upload complete; completion token saved privately.');
