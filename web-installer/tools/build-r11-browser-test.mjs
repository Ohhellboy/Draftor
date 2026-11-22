// Creates a separate synthetic test root. Never modifies a publishable tree.
import {build} from 'esbuild';import {readFile,writeFile,mkdir,cp,stat} from 'node:fs/promises';import path from 'node:path';import {createHash} from 'node:crypto';
const source=process.argv[2],out=process.argv[3];if(!/^dist-r11-public(-dev\d+)?$/.test(source)||!/^dist-r11-test-dev\d+$/.test(out))throw Error('Explicit test root');
try{await stat(out);throw Error('Preserve previous test artifacts');}catch(e){if(e.code!=='ENOENT')throw e;}
await cp(source,out,{recursive:true});await writeFile(`${out}/TEST_ONLY_DO_NOT_DEPLOY.txt`,'Synthetic serial injection. Never publish.\n');
const result=await build({entryPoints:['tests/r11-browser-port.mjs'],bundle:true,format:'iife',write:false,target:['chrome120'],loader:{'.bin':'binary'},alias:{'node:assert/strict':path.resolve('tests/browser-assert.mjs'),'node:crypto':path.resolve('tests/browser-crypto.mjs')}});
const js=result.outputFiles[0].text.replace(/<\/script/gi,'<\\/script');await writeFile(`${out}/assets/test-port.js`,js);
let index=await readFile(`${out}/index.html`,'utf8');index=index.replace('<script type="module" src="assets/app.js">','<script src="assets/test-port.js"></script><script type="module" src="assets/app.js">');await writeFile(`${out}/index.html`,index);
let recovery=await readFile(`${out}/recovery.html`,'utf8');recovery=recovery.replace('<script src="assets/recovery.js">','<script src="assets/test-port.js"></script><script src="assets/recovery.js">');await writeFile(`${out}/recovery.html`,recovery);
let offline=await readFile(`${out}/offline/Draftor_Recovery_R11_Offline.html`,'utf8');const digest=createHash('sha256').update(js).digest('base64');offline=offline.replace("script-src ",`script-src 'sha256-${digest}' `).replace('<script>',`<script>${js}</script><script>`);await writeFile(`${out}/SOFTWARE_TEST_OFFLINE.html`,offline);
console.log(JSON.stringify({source,out,productionAppCopiedUnchanged:true,productionOfflineInlineScriptUnchanged:true,syntheticPortInjection:true,noRealDeviceAccess:true}));
