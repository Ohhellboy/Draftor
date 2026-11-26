// SPDX-License-Identifier: Apache-2.0
import {dict} from './content.js';
import {installationCopy} from './r11-copy.js';
import {downloadPanel} from './download-ui.js';
import {PinnedAssetDownloader,isDownloadNetworkFailure} from './firmware.js';
import {PINS} from './pins.js';
import {MAX_PACKAGE_BYTES} from './recovery-zip.js';
import {requireSafe} from './safety.js';
import {browserIsQualified,platformMessage,publicErrorCode,serialErrorHelp} from './release-platform.js';
import {PublicInstallSession} from './public-install-session.js';
import {Diagnostics} from './diagnostics.js';

export function startPublicUI({build,sourceCommit,offlineToolSha256,requestDriver,offlineToolHref="../offline/Draftor_Recovery_R10_Offline.html",offlineToolName="Draftor_Recovery_R10_Offline.html"}) {
 const $=id=>document.getElementById(id);let lang='zh',phase='IDLE',error='',release,session,diagnostics,lastReceipt,busy=false,activeTask,zip,packageName='',infoKey,closed=false,closing=false,activeOperation;
 const urls=new Set(),t=k=>k==='unsupported'?platformMessage(lang):installationCopy[lang][k]||dict[lang][k]||k;
 const modal=document.createElement('dialog');modal.id='install-dialog';modal.setAttribute('aria-labelledby','install-heading');
 const check=(id,key)=>`<label class="confirm"><input type="checkbox" id="${id}"><span data-r10="${key}"></span></label>`;
 const field=(id,key,accept)=>`<label class="field"><span data-r10="${key}"></span><input type="file" id="${id}" accept="${accept}"></label>`;
 modal.innerHTML=`<div class="dialog-inner"><div class="dialog-title"><h3 id="install-heading" data-r10="heading"></h3><button class="close" id="install-close" aria-label="Close">×</button></div><div class="dialog-body">
 <p id="workflow-status" role="status" aria-live="polite"></p><p id="workflow-error" class="notice" role="alert" hidden></p>
 <section id="connect-section"><p data-r10="prepare"></p>${check('physical-board','board')}${check('prepared-tf','tf')}<button id="connect-read" class="install-primary" data-r10="connect" disabled></button></section>
 <section id="plan-section" hidden><h4 id="plan-title"></h4><p id="plan-summary"></p><p id="original-state" class="notice"></p><details id="plan-details"><summary data-r10="plan"></summary><pre id="plan-json"></pre></details>
 <section id="backup-workflow"><p data-r10="backupHelp"></p><button id="capture-backup" data-r10="capture"></button><button id="save-backup" data-r10="save" disabled></button><p data-r10="saveWarning"></p><p id="package-name"></p><details><summary data-r10="raw"></summary>${field('existing-raw','raw','.bin')}</details>${field('saved-backup','reselect','.zip')}<p id="backup-ready" class="notice" data-r10="saved" hidden></p></section>
 <section id="tool-section"><p data-r10="toolHelp"></p><button id="download-tool" data-r10="tool"></button><button id="tool-problem" data-r10="toolProblem"></button><p id="tool-ready" data-r10="toolReady" hidden></p>${check('tool-saved','toolKeep')}</section>
 <section id="confirm-section">${check('plan-confirmed','planConsent')}<div id="current-app-label">${field('current-app','oldApp','.bin')}</div><button id="execute-plan" class="install-primary" data-r10="execute" disabled></button></section></section>
 <progress id="write-progress" max="1" value="0" hidden></progress><p id="finished-note" class="notice" data-r10="finished" hidden></p><p><a href="recovery.html" data-r10="recoveryLink"></a> · <a href="docs/TF_GUIDE.html">TF</a> · <a href="docs/PUBLIC_STATUS.html" data-r10="scopeLink"></a></p><div class="workflow-actions"><button id="save-receipt" data-r10="receipt" disabled></button><button id="stop-close" data-r10="close"></button></div></div></div>`;
 document.body.append(modal);
 const downloads=downloadPanel({anchor:$('workflow-error'),onChange:()=>render(),onRetry:()=>prepareFiles()});
 const toolDownloads=downloadPanel({anchor:$('download-tool'),prefix:'tool-download',purpose:'offline-tool',onChange:()=>render(),onRetry:()=>prepareOfflineTool(),createLoader:options=>new PinnedAssetDownloader(options),options:{url:new URL(offlineToolHref,import.meta.url),name:offlineToolName,sha256:offlineToolSha256,maxBytes:3999999}});
 const platformNotice=document.createElement('p');platformNotice.id='platform-notice';platformNotice.className='notice';$('install-btn').after(platformNotice);
 const externalReceipt=document.createElement('button');externalReceipt.id='closed-receipt';externalReceipt.dataset.r10='receipt';externalReceipt.hidden=true;document.querySelector('.footer nav').append(externalReceipt);
 function renderInfo(){
  const target=$('dialog-body');
  if(infoKey==='help'||infoKey==='advanced'){
   $('dialog-heading').textContent=t(infoKey==='help'?'helpTitle':'advancedTitle');
   target.replaceChildren();
   for(const text of [t('publicScope'),t('prepare'),t('backupHelp'),t('toolHelp')]){const p=document.createElement('p');p.textContent=text;target.append(p);}
   const links=document.createElement('p');links.innerHTML='<a href="docs/PUBLIC_STATUS.html">范围 / Scope</a> · <a href="docs/TF_GUIDE.html">TF</a> · <a href="recovery.html">备份与恢复 / Recovery</a> · <a href="docs/SOURCE.html">Source & licenses</a>';const windowsLink=document.createElement('a');windowsLink.href=`docs/WINDOWS_FIRST_USER_GUIDE.${lang==='zh'?'zh-CN':'en'}.md`;windowsLink.textContent=lang==='zh'?'Windows 首次使用':'Windows first-use guide';links.append(' · ',windowsLink);target.append(links);
  }else{const c=dict[lang].dialogs[infoKey];$('dialog-heading').textContent=c.title;target.innerHTML=c.body;}
 }
 function render(){
  platformNotice.textContent=platformMessage(lang);document.documentElement.lang=lang==='zh'?'zh-CN':'en';document.title=lang==='zh'?'Draftor 草稿机 · 固件安装':'Draftor · Firmware Installer';
  for(const e of document.querySelectorAll('[data-i18n]'))e.textContent=t(e.dataset.i18n);
  for(const e of document.querySelectorAll('[data-r10]'))e.textContent=t(e.dataset.r10);
  $('board-name').textContent=lang==='zh'?'微雪 ESP32-S3-RLCD-4.2':'Waveshare ESP32-S3-RLCD-4.2';
  const modified=(session?.receipt()||lastReceipt)?.mutationStarted===true;
  downloads.render(lang,['loading','DOWNLOAD_FAILED','DOWNLOAD_CANCELLED'].includes(phase));
  $('workflow-status').textContent=['loading','DOWNLOAD_FAILED','DOWNLOAD_CANCELLED'].includes(phase)?downloads.status(lang):['TOOL_DOWNLOADING','TOOL_DOWNLOAD_FAILED'].includes(phase)?toolDownloads.status(lang):phase==='STOP'?t(modified?'stoppedHelp':'notModified'):installationCopy[lang].phases[phase]||t(phase);
  $('workflow-error').hidden=!error;$('workflow-error').textContent=error?`${error} · ${serialErrorHelp(error,lang)||t(error==='PACKAGE_METADATA'?'wrongOriginal':error.startsWith('PACKAGE_')?'badPackage':modified?'stoppedHelp':'notModified')}`:'';
  $('connect-section').hidden=!!session||closed||!release||!browserIsQualified();
  $('connect-read').disabled=busy||!release||!$('physical-board').checked||!$('prepared-tf').checked;
  const plan=session?.plan;
  if(plan){const blank=plan.initialState==='blank';$('plan-section').hidden=closed;$('plan-title').textContent=t(blank?'blankFresh':plan.mode==='fresh'?'fresh':'update');$('plan-summary').textContent=t(blank?'blankImpact':plan.mode==='fresh'?'freshImpact':'updateImpact');$('plan-json').textContent=JSON.stringify(plan,null,2);
   $('backup-workflow').hidden=blank;$('tool-section').hidden=blank;
   $('original-state').textContent=t(blank?'blank':plan.initialState==='unrecognized'?'unrecognized':plan.mode==='fresh'?'knownFactory':'knownDraftor');
   document.querySelector('#plan-confirmed + span').textContent=t(plan.mode==='fresh'?'eraseConsent':'planConsent');$('current-app-label').hidden=plan.mode==='fresh';}
  const prepared=session?.state==='PLAN_READY'&&!closed;
  for(const id of ['capture-backup','existing-raw'])$(id).disabled=busy||!prepared||!!zip||session?.backupVerified;
  $('save-backup').disabled=busy||!zip||closed;$('saved-backup').disabled=busy||!prepared;
  $('backup-ready').hidden=!session?.backupVerified;$('package-name').textContent=packageName;
  toolDownloads.render(lang,!closed&&!!session&&toolDownloads.snapshot().attempt>0,{canRetry:!busy&&prepared});
  $('download-tool').hidden=!closed&&toolDownloads.snapshot().stage==='FAILED';$('tool-problem').disabled=busy||!prepared;
  $('download-tool').disabled=busy||!prepared;$('tool-ready').hidden=!session?.offlineToolVerified;
  $('tool-saved').disabled=busy||!session?.offlineToolVerified;
  $('execute-plan').disabled=busy||!prepared||!session?.backupVerified||(plan?.initialState!=='blank'&&(!session?.offlineToolVerified||!$('tool-saved').checked))||!['plan-confirmed','prepared-tf'].every(id=>$(id).checked);
  $('save-receipt').disabled=!diagnostics&&!downloads.snapshot().attempt;$('stop-close').textContent=t(busy?'stop':'close');
  if($('info-dialog').open)renderInfo();
 }
 function clearPrivate(){zip?.fill(0);zip=undefined;packageName='';for(const id of ['existing-raw','saved-backup','current-app'])$(id).value='';for(const id of ['physical-board','prepared-tf','plan-confirmed','tool-saved'])$(id).checked=false;for(const u of urls)URL.revokeObjectURL(u);urls.clear();}
 async function releaseSession(){if(closed)return;try{await session?.close();}finally{closed=true;clearPrivate();diagnostics?.check('cleanup',{sessionPrivateReferencesCleared:true,domIdentityCleared:true,fileInputsCleared:true,objectUrlsRevoked:true,physicalMemoryErasureGuaranteed:false});lastReceipt=session?.receipt()||{diagnostics:diagnostics?.snapshot()};session=undefined;$('plan-section').hidden=true;$('plan-json').textContent='';externalReceipt.hidden=!diagnostics&&!downloads.snapshot().attempt;}}
 function download(bytes,name,type){const u=URL.createObjectURL(new Blob([bytes],{type}));urls.add(u);const a=document.createElement('a');a.href=u;a.download=name;document.body.append(a);a.click();a.remove();setTimeout(()=>{URL.revokeObjectURL(u);urls.delete(u);},30000);}
 async function run(fn,operation='device'){if(busy)return;busy=true;activeOperation=operation;render();activeTask=(async()=>{try{await fn();}catch(e){error=publicErrorCode(e);diagnostics?.error({code:error});session?.cancel(error);try{await releaseSession();}catch{error+=' / CLOSE_FAILED';}phase='STOP';}finally{busy=false;activeOperation=undefined;render();}})();return activeTask;}
 function onState(next,data={}){phase=next;if(data.progress!==undefined||data.backupProgress!==undefined){$('write-progress').hidden=false;$('write-progress').value=data.progress??data.backupProgress;}render();}
 async function prepareFiles(){
  if(busy||session||closed||!['loading','DOWNLOAD_FAILED'].includes(phase))return;
  busy=true;activeOperation='firmware';phase='loading';error='';render();
  activeTask=(async()=>{try{release=await downloads.load();phase='ready';}catch(e){release=undefined;phase=e.detail?.category==='CANCELLED'?'DOWNLOAD_CANCELLED':'DOWNLOAD_FAILED';}finally{busy=false;activeOperation=undefined;render();}})();
  return activeTask;
 }
 $('install-btn').addEventListener('click',()=>{if(modal.open||busy)return;closed=false;closing=false;phase=browserIsQualified()?'loading':'unsupported';error='';release=undefined;diagnostics=undefined;lastReceipt=undefined;downloads.reset();toolDownloads.reset();$('plan-section').hidden=true;$('finished-note').hidden=true;$('write-progress').hidden=true;clearPrivate();modal.showModal();render();if(browserIsQualified())prepareFiles();});
 $('connect-read').addEventListener('click',()=>{if($('connect-read').disabled)return;diagnostics=new Diagnostics({build,sourceCommit,originType:location.protocol==='https:'?'HTTPS':'LOOPBACK',origin:location.origin,mode:'installation'});run(async()=>{requireSafe(browserIsQualified(),'PLATFORM');const driver=await requestDriver(diagnostics);session=new PublicInstallSession(driver,release,{build,sourceCommit,offlineToolSha256,platformQualified:browserIsQualified(),diagnostics,onState});await session.prepare({boardConfirmed:$('physical-board').checked,tfRemoved:$('prepared-tf').checked});});});
 $('capture-backup').addEventListener('click',()=>{if(!$('capture-backup').disabled)run(async()=>{zip=await session.captureBackup();packageName=`Draftor-private-${session.plan.mode==='fresh'?'device-original':'draftor-original'}-${new Date().toISOString().replace(/[:.]/g,'-')}.zip`;});});
 $('existing-raw').addEventListener('change',()=>{const f=$('existing-raw').files[0];if(!f)return;run(async()=>{requireSafe(f.size===PINS.flashBytes,'PACKAGE_SIZE');let b;try{b=new Uint8Array(await f.arrayBuffer());zip=await session.importExistingRaw(b);packageName=`Draftor-private-${session.plan.mode==='fresh'?'device-original':'draftor-original'}-${new Date().toISOString().replace(/[:.]/g,'-')}.zip`;}finally{b?.fill(0);$('existing-raw').value='';}});});
 $('save-backup').addEventListener('click',()=>{if(zip&&!busy)download(zip,packageName,'application/zip');});
 $('saved-backup').addEventListener('change',()=>{session?.invalidateSavedPackage();$('plan-confirmed').checked=false;const f=$('saved-backup').files[0];render();if(!f)return;run(async()=>{requireSafe(f.size>PINS.flashBytes&&f.size<=MAX_PACKAGE_BYTES,'PACKAGE_SIZE');let b;try{b=new Uint8Array(await f.arrayBuffer());await session.verifySavedBackup(b);}finally{b?.fill(0);$('saved-backup').value='';}});});
 function prepareOfflineTool(){
  if(busy||closed||closing||session?.state!=='PLAN_READY'||session.receipt().mutationStarted)return;
  return run(async()=>{
   session.invalidateOfflineTool();$('tool-saved').checked=false;phase='TOOL_DOWNLOADING';error='';render();
   let bytes;
   try{
    bytes=await toolDownloads.load();if(closing)return;
    // The network helper checks the exact published SHA; the existing session
    // must independently verify it before allowing the user's saved-file consent.
    await session.verifyOfflineTool(bytes);if(closing)return;
    download(bytes,offlineToolName,'text/html');phase='PLAN_READY';
   }catch(e){
    session?.invalidateOfflineTool();
    if(closing&&e.detail?.category==='CANCELLED')return;
    if(isDownloadNetworkFailure(e)&&session?.state==='PLAN_READY'&&!session.receipt().mutationStarted){phase='TOOL_DOWNLOAD_FAILED';return;}
    if(e.detail)requireSafe(false,'RECOVERY_TOOL');
    throw e;
   }finally{bytes?.fill(0);}
  },'offline-tool');
 }
 $('download-tool').addEventListener('click',()=>{if(!$('download-tool').disabled)prepareOfflineTool();});
 $('tool-problem').addEventListener('click',()=>{session?.invalidateOfflineTool();$('tool-saved').checked=false;render();});
 $('current-app').addEventListener('change',()=>{$('plan-confirmed').checked=false;if(busy){downloads.cancel();toolDownloads.cancel();session?.cancel('PACKAGE_CHANGED');}render();});
 $('execute-plan').addEventListener('click',()=>{if($('execute-plan').disabled)return;run(async()=>{const f=$('current-app').files[0];let b;try{if(f){requireSafe(f.size>=24&&f.size<=PINS.slotCapacity,'CURRENT_APP');b=new Uint8Array(await f.arrayBuffer());}await session.execute({backupConfirmed:session.backupVerified,tfRemoved:$('prepared-tf').checked,planConfirmed:$('plan-confirmed').checked,eraseConfirmed:session.plan.mode==='fresh'&&$('plan-confirmed').checked,offlineToolSaved:$('tool-saved').checked,currentApp:b});await releaseSession();$('finished-note').hidden=false;}finally{b?.fill(0);}});});
 for(const id of ['physical-board','prepared-tf','plan-confirmed','tool-saved'])$(id).addEventListener('change',()=>{if(busy){downloads.cancel();toolDownloads.cancel();session?.cancel('CONSENT_CHANGED');}render();});
 async function close(){closing=true;downloads.cancel();toolDownloads.cancel();if(busy){if(activeOperation==='device')session?.cancel();await activeTask;}try{await releaseSession();downloads.dispose();toolDownloads.dispose();release=undefined;modal.close();}catch{error='CLOSE_FAILED';phase='STOP';render();}}
 $('install-close').addEventListener('click',close);$('stop-close').addEventListener('click',close);modal.addEventListener('cancel',e=>{e.preventDefault();close();});
 function receipt(){const r={schema:'draftor-install-download-receipt/1',build,sourceCommit,flashOutcome:'NO_FLASH_WRITE_ATTEMPT',...(session?.receipt()||lastReceipt),downloadPreparation:downloads.snapshot(),offlineToolDownload:toolDownloads.snapshot()};if(r)download(new TextEncoder().encode(JSON.stringify(r,null,2)+'\n'),'Draftor-install-sanitized-receipt.json','application/json');}
 $('save-receipt').addEventListener('click',receipt);externalReceipt.addEventListener('click',receipt);
 $('lang-toggle').addEventListener('click',()=>{lang=lang==='zh'?'en':'zh';render();});
 $('board-change').addEventListener('click',()=>{$('board-chooser').hidden=!$('board-chooser').hidden;$('board-change').setAttribute('aria-expanded',String(!$('board-chooser').hidden));});
 for(const e of document.querySelectorAll('[data-modal]'))e.addEventListener('click',()=>{infoKey=e.dataset.modal;renderInfo();$('info-dialog').showModal();});
 $('dialog-close').addEventListener('click',()=>$('info-dialog').close());
 addEventListener('beforeunload',e=>{if(busy||session){e.preventDefault();e.returnValue='';}});addEventListener('pagehide',()=>{closing=true;downloads.dispose();toolDownloads.dispose();release=undefined;session?.cancel();session?.close().catch(()=>{});clearPrivate();});render();
}
