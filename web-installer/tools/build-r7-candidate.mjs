// Isolated review artifact only. No deployment operation, no test build flags.
import { build } from 'esbuild';
import { cp, mkdir, readFile, writeFile, readdir, stat } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { verifyReleaseInputs } from './verify-release.mjs';
const out=process.argv[2]||'dist-r7-candidate';
if(!/^dist-r7-candidate(-[a-z0-9]+)?$/.test(out))throw Error('Review directory required');
try{await stat(out);throw Error('Preserve existing artifact');}catch(e){if(e.code!=='ENOENT')throw e;}
let sourceCommit;
try { sourceCommit=JSON.parse(await readFile('SOURCE_IDENTITY.json','utf8')).sourceCommit; }
catch { execFileSync('git',['diff','--exit-code','HEAD','--','.']);sourceCommit=execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim(); }
await verifyReleaseInputs({verifyDesignBrief:false});await mkdir(`${out}/assets`,{recursive:true});
for(const n of ['LICENSE','NOTICE','LICENSES','Notices','vendor','firmware','style.css','docs'])await cp(`public/${n}`,`${out}/${n}`,{recursive:true});
await cp('docs/WEB_LICENSE.md',`${out}/docs/WEB_LICENSE.md`);await cp('docs/RECOVERY_R7.html',`${out}/docs/RECOVERY_R7.html`);
await mkdir(`${out}/sources`);await cp('public/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip',`${out}/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip`);
await writeFile(`${out}/index.html`,(await readFile('index.html','utf8')).replace('网页预览版 · 尚未接入正式烧录程序','R7 待审候选 · 未公开发布 · 可擦写内部 Flash'));
await writeFile(`${out}/docs/SOURCE.html`,(await readFile(`${out}/docs/SOURCE.html`,'utf8')).replace('当前正式网站仅提供只读检查与安装计划，真实写入暂未公开开放。','这是可擦写的待审候选，禁止部署为公开站点。').replace('The public website provides read-only checks and plans. Hardware writing is not yet public.','This review candidate can write after its safety checks. Do not deploy it publicly.'));
await writeFile(`${out}/DO_NOT_DEPLOY.txt`,'REVIEW ONLY. User approval and public-install gates are outstanding. Never replace the locked Pages website with this artifact.\n');
await build({entryPoints:['src/candidate-main.js'],bundle:true,format:'esm',target:['chrome120'],outfile:`${out}/assets/app.js`,sourcemap:true,legalComments:'linked',banner:{js:'/*! R7 REVIEW CANDIDATE — DO NOT DEPLOY. GPL-3.0. Source: ../sources/Draftor_Web_R7_source.zip */'}});
execFileSync('python3',['tools/package-r7-source.py',`${out}/sources/Draftor_Web_R7_source.zip`,sourceCommit],{stdio:'inherit'});
const sha=b=>createHash('sha256').update(b).digest('hex');
const lock=JSON.parse(await readFile('package-lock.json','utf8'));
await writeFile(`${out}/DEPENDENCIES.json`,JSON.stringify(Object.fromEntries(Object.entries(lock.packages).filter(([n])=>n).map(([n,p])=>[n,{version:p.version,integrity:p.integrity,resolved:p.resolved,license:p.license,dev:!!p.dev}])),null,2)+'\n');
await writeFile(`${out}/BUILD.json`,JSON.stringify({version:'R7-candidate-20261121',sourceCommit,publicInstallation:false,reviewOnly:true,approval:'PENDING',authorization:'single mode granted after device inspection and saved backup verification',initialUpdateCapability:false,initialFreshCapability:false,eligible:['exact alpha.3 app0 same-version','exact FactoryOnly 20260120'],refused:['app1','unknown firmware','unknown factory','blank hardware writes'],appBundleSha256:sha(await readFile(`${out}/assets/app.js`))},null,2)+'\n');
const rows=[];async function walk(dir){for(const e of(await readdir(dir,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){const p=`${dir}/${e.name}`;if(e.isDirectory())await walk(p);else rows.push(`${sha(await readFile(p))}  ${p.slice(out.length+1)}`);}}
await walk(out);await writeFile(`${out}/SITE_SHA256SUMS`,rows.join('\n')+'\n');console.log(JSON.stringify({out,sourceCommit,files:rows.length,manifestSha256:sha(await readFile(`${out}/SITE_SHA256SUMS`))}));
