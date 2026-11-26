// SPDX-License-Identifier: Apache-2.0
import {installationCopy} from './r11-copy.js';
import {dict} from './content.js';import {productCopy} from './r12-copy.js';
import {loadRelease} from './firmware.js';import {verifiedFactory} from './install-protection.js';
import {ProductInstallSession} from './product-install-session.js';import {chooseProtectionFile,saveAndReadBack} from './save-protection.js';
import {Diagnostics} from './diagnostics.js';import {requireSafe} from './safety.js';
import {browserIsQualified,platformMessage,publicErrorCode,serialErrorHelp} from './release-platform.js';
const app0Copy = {
 zh: {intro:'通过 USB 连接原微雪设备，重新安装当前版本。',previewHint:'微雪 ST7305 · Alpha 3 app0 同版本重装',finished:'安装内容已校验，连接已关闭。彻底断电（含电池），插回原 TF，松开 BOOT 普通开机。'},
 en: {intro:'Connect your Waveshare board via USB to reinstall its current version.',previewHint:'Waveshare ST7305 · Alpha 3 app0 reinstall',finished:'Installation verified and connection closed. Power off completely, including the battery, insert the original TF card and boot with BOOT released.'}
};
const publicCopy = {
 zh: {
  installBtn:'重新安装 Alpha 3',firstInstallLink:'第一次安装／清空安装',testerGuide:'安装操作说明',
  intro:'已在用 Alpha 3，可尝试“重新安装 Alpha 3”保留数据。其他固件或希望清空设备，请选“第一次安装／清空安装”。',
  previewHint:'微雪 ST7305 · 初次安装或同版本重装',
  prepared:'点击即确认上述准备已完成。请选择这块板对应的 USB 设备。Windows 可能显示 USB 串行设备（COM 编号）。',
  protection:'把小型保护文件保存在自己的电脑上。写入前，网页会自动读回并核对。请保留文件，它记录这块板原来的状态，供安装中断时排查。完整备份可在“完整备份与原系统恢复”中单独保存。',
  exception:'若写入中断，请保留保护文件和脱敏回执，打开“安装中断处理”。'
 },
 en: {
  installBtn:'Reinstall Alpha 3',firstInstallLink:'First-time / clean installation',testerGuide:'Installation guide',
  intro:'Choose Reinstall Alpha 3 to preserve compatible data. For other firmware or a clean start, choose First-time / clean installation.',
  previewHint:'Waveshare ST7305 · first installation or same-version reinstall',
  prepared:'Continue once the board is ready. Select its USB device. Windows may show USB Serial Device with a COM number.',
  protection:'Save the small protection file on your computer. The site reads it back before writing. Keep it for investigating an interrupted installation on this board. You can save a full backup separately under Full backup & original-system recovery.',
  exception:'If writing is interrupted, keep the protection file and sanitized receipt and open Interrupted installation.'
 }
};
export function startProductUI({build,sourceCommit,requestDriver,reviewOnly=false,app0Only=false,publicSite=false}){
 const $=id=>document.getElementById(id);let lang='zh',phase='ready',busy=false,task,session,release,factory,point,diag,receipt,error='',cancelled=false,closed=true,saved=false,infoKey;
 const urls=new Set(),t=k=>(publicSite && k==='prepare' && installationCopy[lang].prepare)||(publicSite && publicCopy[lang][k])||(app0Only && app0Copy[lang][k])||productCopy[lang][k]||dict[lang][k]||k;
 const interruptedHref=publicSite?'guide.html#failure':'interrupted.html';
 const modal=document.createElement('dialog');modal.id='install-dialog';modal.setAttribute('aria-labelledby','install-heading');
 modal.innerHTML=`<div class="dialog-inner"><div class="dialog-title"><h3 id="install-heading" data-r12="heading"></h3><button class="close" id="install-close" aria-label="Close">×</button></div><div class="dialog-body"><p id="workflow-status" role="status" aria-live="polite"></p><p id="workflow-error" class="notice" hidden></p><p id="clean-alternative" hidden></p>
 <section id="connect-section"><p data-r12="prepare"></p><button id="connect-read" class="install-primary" data-r12="connect"></button><p data-r12="prepared"></p></section>
 <section id="plan-section" hidden><h4 id="plan-title"></h4><p id="plan-impact" class="notice"></p><p data-r12="protection"></p><button id="save-install" class="install-primary"></button><section id="save-fallback" hidden><p data-r12="fallback"></p><button id="download-point" data-r12="download"></button><label class="field"><span data-r12="reselect"></span><input type="file" accept=".json" id="saved-point"></label><p id="point-saved" data-r12="saved" hidden></p><button id="fallback-install" class="install-primary" data-r12="fallbackGo" disabled></button></section>
 <p data-r12="exception"></p><details><summary data-r12="details"></summary><pre id="plan-json"></pre></details></section>
 <progress id="write-progress" value="0" max="1" hidden></progress><p id="finished-note" data-r12="finished" class="notice" hidden></p><p><a href="${interruptedHref}" target="_blank" rel="noopener" data-r12="repair"></a> · <a href="recovery.html" data-r12="independent"></a></p><div class="workflow-actions"><button id="save-receipt" data-r12="receipt" disabled></button><button id="stop-close" data-r12="close"></button></div></div></div>`;document.body.append(modal);
 const notice=document.createElement('p');notice.className='install-hint';$('install-btn').after(notice);
 const external=document.createElement('button');external.hidden=true;external.id='closed-receipt';external.dataset.r12='receipt';document.querySelector('.footer nav').append(external);
 function info(){const c=dict[lang].dialogs[infoKey];$('dialog-heading').textContent=c.title;if(infoKey==='help'||infoKey==='advanced'){$('dialog-body').replaceChildren();for(const text of [t('prepare'),t('protection'),t('exception')]){const p=document.createElement('p');p.textContent=text;$('dialog-body').append(p);}const p=document.createElement('p');p.innerHTML='<a href="recovery.html">完整备份与恢复 / Full backup & recovery</a> · <a href="'+interruptedHref+'">安装中断 / Interrupted install</a> · <a href="docs/TF_GUIDE.html">TF</a>';if(publicSite){const guide=document.createElement('a');guide.href='guide.html';guide.textContent=t('testerGuide');p.prepend(guide,' · ');}$('dialog-body').append(p);}else $('dialog-body').innerHTML=c.body;}
 function render(){document.documentElement.lang=lang==='zh'?'zh-CN':'en';document.title=lang==='zh'?'Draftor 草稿机 · 固件安装':'Draftor · Firmware Installer';$('board-name').textContent=lang==='zh'?'微雪 ESP32-S3-RLCD-4.2':'Waveshare ESP32-S3-RLCD-4.2';$('lang-toggle').setAttribute('aria-label',lang==='zh'?'Switch language to English':'切换到中文');for(const e of document.querySelectorAll('[data-i18n]'))e.textContent=t(e.dataset.i18n);for(const e of document.querySelectorAll('[data-r12]'))e.textContent=t(e.dataset.r12);
  if(build.includes('SOFTWARE'))$('finished-note').textContent=lang==='zh'?'软件模拟内容已校验，合成连接已关闭。本轮不要操作真实微雪或 TF；这不代表真实设备或普通启动通过。':'Software content verified; synthetic connection closed. Do not operate a real board or TF. This does not qualify real hardware or normal boot.';
  notice.textContent=browserIsQualified()?(app0Only?'':lang==='zh'?'桌面 Chrome / Edge。Windows 真设备仍待验收；R12 本轮仅软件候选。':'Desktop Chrome / Edge. Windows hardware remains pending; R12 is a software candidate.'):platformMessage(lang);notice.hidden=!notice.textContent;$('workflow-status').textContent=productCopy[lang].phases[phase]||phase;
  const changed=(session?.receipt()||receipt)?.mutationStarted===true;
  const needsClean=publicSite&&['UNKNOWN','PARTITIONS','OTA','UNQUALIFIED_SLOT','UNQUALIFIED_FIRMWARE','FACTORY_FINGERPRINT','BUILD_SCOPE','STALE_PLAN'].includes(error);
  $('clean-alternative').hidden=!needsClean;
  $('clean-alternative').replaceChildren();
  if(needsClean) {
   const link=document.createElement('a');link.href='first-install.html';
   link.textContent=lang==='zh'?'打开“备份后清空安装”':'Open backup and clean installation';
   $('clean-alternative').append(lang==='zh'?'当前固件无法确认保留数据重装兼容性。':'Data-preserving reinstall compatibility could not be established. ',link);
  }
  $('workflow-error').hidden=!error;
  $('workflow-error').textContent=error?error+' · '+(changed?installationCopy[lang].stoppedHelp:installationCopy[lang].notModified)+(serialErrorHelp(error,lang)||''):'';

  $('connect-section').hidden=!!session||closed;const ready=session?.state==='PLAN_READY'&&!closed;
  $('connect-read').disabled=busy||!release||reviewOnly||!browserIsQualified();$('save-install').disabled=busy||!ready;$('save-install').hidden=typeof showSaveFilePicker!=='function';$('save-fallback').hidden=typeof showSaveFilePicker==='function';
  $('plan-section').hidden=!session?.plan||closed||phase==='STOP';if(session?.plan){const fresh=session.plan.mode==='fresh';$('plan-title').textContent=t(fresh?'fresh':'update');$('plan-impact').textContent=t(fresh?'freshImpact':'updateImpact');$('save-install').textContent=t(fresh?'freshAction':'updateAction');$('plan-json').textContent=JSON.stringify(session.plan,null,2);}
  $('download-point').disabled=busy||!ready;$('saved-point').disabled=busy||!ready;$('fallback-install').disabled=busy||!ready||!saved;$('fallback-install').textContent=session?.plan?.mode==='fresh'?(lang==='zh'?'保护文件已核验，清空内部 Flash 并安装':'Protection verified — erase internal Flash and install'):t('fallbackGo');$('point-saved').hidden=!saved;$('save-receipt').disabled=!diag;$('stop-close').textContent=t(busy?'stop':'close');if($('info-dialog').open)info();
 }
 function download(bytes,name,type){const u=URL.createObjectURL(new Blob([bytes],{type}));urls.add(u);const a=document.createElement('a');a.href=u;a.download=name;a.click();setTimeout(()=>{URL.revokeObjectURL(u);urls.delete(u);},30000);}
 function alive(){requireSafe(!closed&&!cancelled,'CANCELLED');}
 async function cleanup(){if(closed)return;try{await session?.close();}finally{closed=true;point?.fill(0);point=undefined;factory?.fill(0);factory=undefined;$('saved-point').value='';$('plan-json').textContent='';for(const u of urls)URL.revokeObjectURL(u);urls.clear();diag?.check('cleanup',{sessionPrivateReferencesCleared:true,fileInputsCleared:true,objectUrlsRevoked:true,physicalMemoryErasureGuaranteed:false});receipt=session?.receipt()||{diagnostics:diag?.snapshot()};external.hidden=!diag;session=undefined;}}
 function onState(s,r){phase=s;if(r.progress!==undefined){$('write-progress').hidden=false;$('write-progress').value=r.progress;}render();}
 async function run(fn){if(busy)return;busy=true;render();task=(async()=>{try{await fn();}catch(e){const outcome=session?.receipt().flashOutcome;error=e.name==='AbortError'&&!outcome?'SAVE_CANCELLED':typeof e.code==='string'?e.code:publicErrorCode(e);session?.cancel(error);try{await cleanup();}catch{error='CLOSE_FAILED';}phase='STOP';}finally{busy=false;render();}})();return task;}
 async function install(){alive();await session.execute({tfRemoved:true,planConfirmed:true,eraseConfirmed:session.plan.mode==='fresh'});await cleanup();$('finished-note').hidden=false;}
 $('install-btn').addEventListener('click',()=>{if(busy||modal.open)return;closed=false;cancelled=false;saved=false;phase='loading';error='';release=undefined;diag=undefined;receipt=undefined;$('finished-note').hidden=true;$('write-progress').hidden=true;modal.showModal();run(async()=>{release=await loadRelease(new URL('../firmware/',import.meta.url));const r=await fetch(new URL('../factory/FactoryOnly.bin',import.meta.url));requireSafe(r.ok,'FACTORY_ASSET');factory=await verifiedFactory(new Uint8Array(await r.arrayBuffer()));alive();phase='ready';if(reviewOnly)error='REVIEW_ONLY_NO_DEVICE_ACCESS';});});
 $('connect-read').addEventListener('click',()=>{if($('connect-read').disabled)return;diag=new Diagnostics({build,sourceCommit,originType:publicSite?'HTTPS':'LOOPBACK',origin:location.origin,mode:'installation',simulation:build.includes('SOFTWARE')});run(async()=>{const driver=await requestDriver(diag);alive();session=new ProductInstallSession(driver,release,{platformQualified:browserIsQualified(),diagnostics:diag,onState});await session.prepare({boardConfirmed:true,tfRemoved:true,factory,app0Only});point=session.protectionFile();});});
 $('save-install').addEventListener('click',()=>{if($('save-install').disabled)return;let picker;try{picker=chooseProtectionFile();}catch(e){picker=Promise.reject(e);}run(async()=>{phase='SAVING';render();let readback;try{const h=await picker;alive();readback=await saveAndReadBack(h,point,{alive});await session.verifySavedProtection(readback);alive();saved=true;await install();}finally{readback?.fill(0);}});});
 $('download-point').addEventListener('click',()=>{if(!busy&&point)download(point,`Draftor-install-protection-${session.plan.mode}-${Date.now()}.json`,'application/json');});
 $('saved-point').addEventListener('change',()=>{saved=false;const f=$('saved-point').files[0];if(!f)return;run(async()=>{requireSafe(f.size<70000,'PROTECTION_SIZE');const b=new Uint8Array(await f.arrayBuffer());try{await session.verifySavedProtection(b);saved=true;}finally{b.fill(0);$('saved-point').value='';}});});
 $('fallback-install').addEventListener('click',()=>{if(!$('fallback-install').disabled)run(install);});
 async function close(){cancelled=true;if(busy){session?.cancel();await task;}await cleanup();modal.close();render();}
 $('install-close').addEventListener('click',close);$('stop-close').addEventListener('click',close);modal.addEventListener('cancel',e=>{e.preventDefault();close();});
 function saveReceipt(){const r=session?.receipt()||receipt;if(r)download(new TextEncoder().encode(JSON.stringify(r,null,2)+'\n'),'Draftor-R12-sanitized-receipt.json','application/json');}external.addEventListener('click',saveReceipt);$('save-receipt').addEventListener('click',saveReceipt);
 $('lang-toggle').addEventListener('click',()=>{lang=lang==='zh'?'en':'zh';render();});$('board-change').addEventListener('click',()=>{$('board-chooser').hidden=!$('board-chooser').hidden;});
 for(const e of document.querySelectorAll('[data-modal]'))e.addEventListener('click',()=>{infoKey=e.dataset.modal;info();$('info-dialog').showModal();});$('dialog-close').addEventListener('click',()=>$('info-dialog').close());
 addEventListener('beforeunload',e=>{if(session||busy){e.preventDefault();e.returnValue='';}});addEventListener('pagehide',()=>{session?.cancel();session?.close().catch(()=>{});point?.fill(0);factory?.fill(0);});render();
}
