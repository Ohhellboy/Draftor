import { build } from 'esbuild';
import { cp, readFile, writeFile, mkdir, readdir, stat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { verifyReleaseInputs } from './verify-release.mjs';
import { PINS } from '../src/pins.js';
const out = process.argv[2] || 'dist-r7-locked';
if (!/^dist-r7-(locked|candidate)(-[a-z0-9]+)?$/.test(out)) throw new Error('Dedicated R7 output required');
if (out.includes('candidate')) throw new Error('Candidate builder not yet approved');
try { await stat(out); throw new Error('Preserve existing build; select a new output name'); } catch (e) { if (e.code !== 'ENOENT') throw e; }
let sourceCommit;
try { sourceCommit = JSON.parse(await readFile('SOURCE_IDENTITY.json', 'utf8')).sourceCommit; }
catch { execFileSync('git', ['diff', '--exit-code', 'HEAD', '--', '.']); sourceCommit = execFileSync('git', ['rev-parse', 'HEAD'], {encoding:'utf8'}).trim(); }
await verifyReleaseInputs({ verifyDesignBrief: false });
await mkdir(`${out}/assets`, {recursive:true});
for (const name of ['LICENSE','NOTICE','LICENSES','Notices','vendor','firmware','style.css','docs']) await cp(`public/${name}`, `${out}/${name}`, {recursive:true});
await mkdir(`${out}/sources`);
await cp('public/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip', `${out}/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip`);
await cp('docs/WEB_LICENSE.md', `${out}/docs/WEB_LICENSE.md`);
let html = await readFile('index.html','utf8');
html = html.replace('通过 USB 连接设备，即可在浏览器中安装固件。无需额外下载软件。','提供 USB 只读检查与安装计划。真实安装暂未公开开放。').replace('网页预览版 · 尚未接入正式烧录程序','正式网站 · 真实写入已锁定');
await writeFile(`${out}/index.html`, html);
await writeFile(`${out}/CNAME`, 'draftor.tcssn.org\n');
await writeFile(`${out}/.nojekyll`, '');
await build({entryPoints:['src/public-main.js'],bundle:true,format:'esm',target:['chrome120'],outfile:`${out}/assets/app.js`,minify:false,sourcemap:true,legalComments:'linked',
 banner:{js:'/*! Draftor Web R7 LOCKED. Combined browser program GPL-3.0. Corresponding source: ../sources/Draftor_Web_R7_source.zip ; licenses: ../docs/SOURCE.html */'}});
execFileSync('python3',['tools/package-r7-source.py',`${out}/sources/Draftor_Web_R7_source.zip`,sourceCommit],{stdio:'inherit'});
const hash = b => createHash('sha256').update(b).digest('hex');
const lock = JSON.parse(await readFile('package-lock.json','utf8'));
await writeFile(`${out}/DEPENDENCIES.json`,JSON.stringify(Object.fromEntries(Object.entries(lock.packages).filter(([n])=>n).map(([n,p])=>[n,{version:p.version,integrity:p.integrity,resolved:p.resolved,license:p.license,dev:!!p.dev}])),null,2)+'\n');
const manifest = {version:'R7-locked-20261121',sourceCommit,buildTime:'2026-11-21T00:00:00Z',buildTimePolicy:'fixed release epoch for reproducibility; actual deployment time recorded in Pages',
 release:PINS.version,firmwareSourceCommit:PINS.commit,hardwareUpdateWrites:false,hardwareFreshWrites:false,publicInstallation:false,experimental:false,
 appBundleSha256:hash(await readFile(`${out}/assets/app.js`)),correspondingSourceSha256:hash(await readFile(`${out}/sources/Draftor_Web_R7_source.zip`)),firmware:PINS.files,
 qualification:{readOnly:'R3_PASS',app0SameVersion:'R4_PASS_INHERITED',exactFactoryCoreInstall:'R6_CORE_PASS_INHERITED',app1:'UNVERIFIED',windowsEdgeHardware:'UNVERIFIED',freshTf:'PENDING',uart0LearningIndex:'UNVERIFIED',mainlandDownloads:'UNVERIFIED'},
 runtimeNetwork:'same-origin static files only; explicit TF downloads from pinned GitHub release URLs',dependencies:JSON.parse(await readFile('package.json','utf8')).dependencies};
await writeFile(`${out}/BUILD.json`,JSON.stringify(manifest,null,2)+'\n');
const rows=[];
async function walk(dir) { for(const e of (await readdir(dir,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))) { const p=`${dir}/${e.name}`;if(e.isDirectory())await walk(p);else {if(e.isSymbolicLink())throw Error('No symlinks'); rows.push(`${hash(await readFile(p))}  ${p.slice(out.length+1)}`);} } }
await walk(out);await writeFile(`${out}/SITE_SHA256SUMS`,rows.join('\n')+'\n');
console.log(JSON.stringify({out,files:rows.length,manifestSha256:hash(await readFile(`${out}/SITE_SHA256SUMS`)),sourceCommit}));
