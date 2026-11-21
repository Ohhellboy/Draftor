// SPDX-License-Identifier: Apache-2.0
// Read-only publishing gate. A candidate, test build or mixed file set is rejected.
import {readFile,readdir,lstat,realpath} from 'node:fs/promises';import {createHash} from 'node:crypto';import {fileURLToPath} from 'node:url';import path from 'node:path';
const hash=b=>createHash('sha256').update(b).digest('hex');
export async function validateLockedSite(input){
 const root=await realpath(input),manifest=JSON.parse(await readFile(path.join(root,'BUILD.json'),'utf8'));
 if(manifest.frozen!==true||manifest.reviewOnly!==false||manifest.hardwareUpdateWrites!==false||manifest.hardwareFreshWrites!==false||manifest.hardwareRecoveryWrites!==false||manifest.publicInstallation!==false)throw Error('R9_PUBLISH_WRITER_OR_UNFROZEN');
 const rows=new Map();for(const line of(await readFile(path.join(root,'SITE_SHA256SUMS'),'utf8')).trim().split('\n')){
  const match=/^([0-9a-f]{64})  ([A-Za-z0-9_. /-]+)$/.exec(line);if(!match||match[2].startsWith('/')||match[2].split('/').includes('..')||rows.has(match[2]))throw Error('R9_PUBLISH_MANIFEST');rows.set(match[2],match[1]);
 }
 const seen=[];async function walk(dir){for(const e of await readdir(dir,{withFileTypes:true})){const p=path.join(dir,e.name);if(e.isSymbolicLink())throw Error('R9_PUBLISH_SYMLINK');if(e.isDirectory())await walk(p);else{const n=path.relative(root,p).split(path.sep).join('/');if(n!=='SITE_SHA256SUMS'){if(hash(await readFile(p))!==rows.get(n))throw Error('R9_PUBLISH_CONTENT');seen.push(n);}}}}
 await walk(root);if(seen.length!==rows.size||seen.some(n=>/DO_NOT_DEPLOY|UNFROZEN|^mock\/|private|receipt/i.test(n)))throw Error('R9_PUBLISH_MIXED_FILES');
 if(await readFile(path.join(root,'CNAME'),'utf8')!=='draftor.tcssn.org\n')throw Error('R9_PUBLISH_DOMAIN');
 for(const name of ['assets/app.js','assets/recovery.js','offline/Draftor_Recovery_R9_Offline.html']){
  const b=await readFile(path.join(root,name),'utf8');if(/allowRecovery:\s*true|allowUpdate:\s*true|allowFirstInstall:\s*true|candidate:\s*true|simulation:\s*true|SOFTWARE-TEST-ONLY/.test(b)||!b.includes('ReadOnlyDriver'))throw Error('R9_PUBLISH_CAPABILITY');
 }
 const b=await readFile(path.join(root,'assets/recovery.js'));if(hash(b)!==manifest.recoveryBundleSha256)throw Error('R9_PUBLISH_BUNDLE');
 return {result:'LOCKED_SITE_ELIGIBLE_FOR_REVIEW_NOT_DEPLOYED',sourceCommit:manifest.sourceCommit,files:seen.length+1,manifestSha256:hash(await readFile(path.join(root,'SITE_SHA256SUMS')))};
}
if(process.argv[1]===fileURLToPath(import.meta.url))console.log(JSON.stringify(await validateLockedSite(process.argv[2])));
