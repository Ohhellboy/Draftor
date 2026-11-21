// Download only the pinned public experiment fixture. No device APIs.
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { FACTORY } from '../src/factory-pins.js';
const root = new URL('../release-inputs/factory/', import.meta.url);
const name = FACTORY.file.split('/').at(-1);
function verify(data) {
  if (data.length !== FACTORY.bytes || createHash('sha256').update(data).digest('hex') !== FACTORY.sha256 ||
      createHash('sha1').update(`blob ${data.length}\0`).update(data).digest('hex') !== FACTORY.git_blob_sha1) throw new Error('Factory fixture integrity mismatch');
}
try { const cached=await readFile(new URL(name,root)); verify(cached); console.log('Verified existing pinned factory fixture.'); }
catch(error) {
  if(error.code!=='ENOENT') throw error;
  const response=await fetch(`https://raw.githubusercontent.com/waveshareteam/ESP32-S3-RLCD-4.2/${FACTORY.commit}/${FACTORY.file}`,{redirect:'error',signal:AbortSignal.timeout(120000)});
  if(!response.ok || !response.body)throw new Error('Factory download failed');
  const bytes=new Uint8Array(FACTORY.bytes); let at=0;
  for await(const chunk of response.body){if(at+chunk.length>bytes.length)throw new Error('Oversized factory asset');bytes.set(chunk,at);at+=chunk.length;}
  if(at!==bytes.length)throw new Error('Truncated factory asset');verify(bytes);
  await mkdir(root,{recursive:true});await writeFile(new URL(name,root),bytes,{flag:'wx'});
  console.log('Pinned factory fixture SHA-256 and Git blob verified.');
}
