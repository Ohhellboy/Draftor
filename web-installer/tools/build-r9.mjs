// SPDX-License-Identifier: Apache-2.0
import {build} from 'esbuild';
import {readFile,writeFile,mkdir,cp,stat,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';import {execFileSync} from 'node:child_process';
import {verifyReleaseInputs} from './verify-release.mjs';import {PINS} from '../src/pins.js';
const kind=process.argv[2],out=process.argv[3],development=process.argv.includes('--development');
if(!['locked','recovery'].includes(kind)||!/^dist-r9-(locked|recovery)(-[a-z0-9]+)?$/.test(out)||!out.startsWith(`dist-r9-${kind}`))throw Error('Explicit, dedicated R9 output required');
try{await stat(out);throw Error('Output exists: preserve frozen builds and use a new suffix');}catch(e){if(e.code!=='ENOENT')throw e;}
let sourceCommit;
try{sourceCommit=JSON.parse(await readFile('SOURCE_IDENTITY.json','utf8')).sourceCommit;}catch{if(!development)execFileSync('git',['diff','--exit-code','HEAD','--','.']);sourceCommit=execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim();}
await verifyReleaseInputs({verifyDesignBrief:false});
const buildId=`R9-${kind}${development?'-UNFROZEN':''}-${sourceCommit.slice(0,12)}`;
const hash=b=>createHash('sha256').update(b).digest('hex');const shaBase64=b=>createHash('sha256').update(b).digest('base64');
await mkdir(`${out}/assets`,{recursive:true});
for(const n of ['LICENSE','NOTICE','LICENSES','Notices','vendor','firmware','style.css','recovery.css','docs'])await cp(`public/${n}`,`${out}/${n}`,{recursive:true});
await mkdir(`${out}/sources`);await cp('public/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip',`${out}/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip`);
for(const lang of ['zh-CN','en'])await cp(`docs/RECOVERY_GUIDE.${lang}.md`,`${out}/docs/RECOVERY_GUIDE.${lang}.md`);
let index=await readFile('index.html','utf8');
index=index.replace('网页预览版 · 尚未接入正式烧录程序','R9 · 真实安装写入锁定').replace('<details class="tf-downloads"','<p class="install-hint"><a href="recovery.html" data-i18n="recoveryLink">设备无法启动？恢复设备 / 保存备份</a></p>\n      <details class="tf-downloads"');
await writeFile(`${out}/index.html`,index);await cp('recovery.html',`${out}/recovery.html`);
const defines={__R9_BUILD__:JSON.stringify(buildId),__R9_SOURCE__:JSON.stringify(sourceCommit)};
const config={bundle:true,target:['chrome120'],minify:false,legalComments:'inline',define:defines};
await build({...config,entryPoints:['src/r9-locked-main.js'],format:'esm',outfile:`${out}/assets/app.js`,sourcemap:true});
const recoveryEntry=`src/r9-recovery-${kind==='locked'?'locked':'candidate'}.js`;
await build({...config,entryPoints:[recoveryEntry],format:'iife',outfile:`${out}/assets/recovery.js`,sourcemap:true});
// No module fetches, runtime imports, CDN, service worker or cached-page dependency.
const inline=await build({...config,entryPoints:[recoveryEntry],format:'iife',write:false});
const js=inline.outputFiles[0].text.replace(/<\/script/gi,'<\\/script');
const css=await readFile('public/style.css','utf8')+'\n'+await readFile('public/recovery.css','utf8');
let offline=await readFile('recovery.html','utf8');
offline=offline.replace(/<link rel="stylesheet"[^>]*>/g,'').replace('<script src="assets/recovery.js"></script>',`<script>${js}</script>`).replace('</head>',`<style>${css}</style></head>`)
 .replace("script-src 'self'",`script-src 'sha256-${shaBase64(js)}'`).replace("style-src 'self'",`style-src 'sha256-${shaBase64(css)}'`).replace('href="index.html"','href="#"');
const licenseNames=['GNU_GPL_v3.txt','esptool_GPL-2.0.txt','esptool-js-LICENSE','spark-md5-LICENSE'];
const escape=s=>s.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
const licenses=await Promise.all(licenseNames.map(async n=>`<details><summary>${n}</summary><pre>${escape(await readFile(`public/LICENSES/${n}`,'utf8'))}</pre></details>`));
offline=offline.replace('</main>',`<section id="offline-licenses"><h2>Source &amp; licenses</h2><p>Combined program: GPL-3.0. Corresponding source: Draftor_Web_R9_source.zip in this delivery. Build ${buildId}; source ${sourceCommit}. No private package is included.</p>${licenses.join('')}</section></main>`);
await mkdir(`${out}/offline`);await writeFile(`${out}/offline/Draftor_Recovery_R9_Offline.html`,offline);
if(kind==='locked'){await writeFile(`${out}/CNAME`,'draftor.tcssn.org\n');await writeFile(`${out}/.nojekyll`,'');}
else await writeFile(`${out}/DO_NOT_DEPLOY.txt`,'REVIEW ONLY: recovery writer. Never publish this directory or offline candidate without a separate public-release decision.\n');
if(development)await writeFile(`${out}/UNFROZEN.txt`,'Software development build. Not a frozen device-test or deployment artifact.\n');
else execFileSync('/Library/Frameworks/Python.framework/Versions/3.14/bin/python3',['tools/package-r9-source.py',`${out}/sources/Draftor_Web_R9_source.zip`,sourceCommit],{stdio:'inherit'});
await writeFile(`${out}/docs/SOURCE.html`,`<!doctype html><meta charset="utf-8"><title>Source & licenses</title><h1>Draftor R9</h1><p>Combined browser program: GPL-3.0; component notices retained.</p><p><a href="../sources/Draftor_Web_R9_source.zip">Corresponding source, lockfile, tests and build tools</a></p><p><a href="../LICENSES/GNU_GPL_v3.txt">GPL-3.0</a> · <a href="../LICENSES/esptool_GPL-2.0.txt">esptool</a> · <a href="../LICENSES/esptool-js-LICENSE">esptool-js</a> · <a href="../LICENSES/spark-md5-LICENSE">spark-md5</a></p><p>Source commit: ${sourceCommit}</p>`);
const manifest={version:buildId,sourceCommit,frozen:!development,reviewOnly:kind==='recovery',hardwareUpdateWrites:false,hardwareFreshWrites:false,hardwareRecoveryWrites:kind==='recovery',publicInstallation:false,
 schema:'draftor-private-recovery/1',release:PINS.version,appSha256:PINS.files['app.bin'].sha256,appBundleSha256:hash(await readFile(`${out}/assets/app.js`)),recoveryBundleSha256:hash(await readFile(`${out}/assets/recovery.js`)),offlineHtmlSha256:hash(offline),
 qualification:{recoverySoftware:'SEE_R9_REPORT',recoveryDevice:'PENDING',publicRecovery:'NOT_AUTHORIZED',windows:'NOT_TESTED'},dependencies:JSON.parse(await readFile('package.json','utf8')).dependencies};
await writeFile(`${out}/BUILD.json`,JSON.stringify(manifest,null,2)+'\n');
const rows=[];async function walk(dir){for(const e of(await readdir(dir,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){const p=`${dir}/${e.name}`;if(e.isSymbolicLink())throw Error('Symlink forbidden');if(e.isDirectory())await walk(p);else rows.push(`${hash(await readFile(p))}  ${p.slice(out.length+1)}`);}}
await walk(out);await writeFile(`${out}/SITE_SHA256SUMS`,rows.join('\n')+'\n');console.log(JSON.stringify({out,buildId,sourceCommit,files:rows.length,siteManifestSha256:hash(await readFile(`${out}/SITE_SHA256SUMS`))}));
