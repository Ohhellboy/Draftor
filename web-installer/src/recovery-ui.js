// SPDX-License-Identifier: Apache-2.0
import { RecoverySession } from './recovery-session.js';
import { validateRecoveryPackage,recoverySummary,destroyRecoveryPackage } from './recovery-package.js';
import { MAX_PACKAGE_BYTES } from './recovery-zip.js';
import { requireSafe } from './safety.js';
import { Diagnostics } from './diagnostics.js';
import { recoveryCopy } from './recovery-copy.js';

export function startRecoveryUI({build,sourceCommit,recoveryEnabled=false,simulation=false,requestDriver,copyOverrides={},supportedEnvironment,offlineHref,embeddedGuide=false,mapError,errorHelp}) {
  const $=id=>document.getElementById(id);let language='zh',mode=recoveryEnabled?'restore':'backup',session,diagnostics,packageBytes,downloadBytes,summary,busy=false,activeTask,notice='',error='',closed=false;
  const copy=Object.fromEntries(['zh','en'].map(l=>[l,{...recoveryCopy[l],...copyOverrides[l]}]));
  const objectUrls=new Set();const t=key=>copy[language][key]||key;let seenOpened=false;
  const originType=location.protocol==='file:'?'FILE':location.protocol==='https:'?'HTTPS':'LOOPBACK';
  const supported=supportedEnvironment??(simulation||(isSecureContext&&!!navigator.serial&&!/Android|iPhone|iPad|Mobile/i.test(navigator.userAgent)));
  const newDiagnostics=()=>new Diagnostics({build,sourceCommit,originType,origin:originType==='FILE'?'LOCAL_FILE':location.origin,mode,simulation});
  diagnostics=newDiagnostics();diagnostics.check('writeLock',!recoveryEnabled||mode!=='restore');
  function receipt() {
    const data=diagnostics.snapshot(),attempted=data.commands.FLASH_BEGIN.attempted+data.commands.FLASH_DATA.attempted;
    return {schema:'draftor-recovery-receipt/1',diagnostics:data,flashOutcome:attempted?(data.stage==='VERIFIED_BOOT_PENDING'?'VERIFIED_BOOT_PENDING':'POSSIBLY_PARTIAL'):'NO_FLASH_WRITE_ATTEMPT',normalBoot:'NOT_VERIFIED_BY_TOOL',tf:'NOT_ACCESSED'};
  }
  function render() {
    document.documentElement.lang=language==='zh'?'zh-CN':'en';for(const el of document.querySelectorAll('[data-t]'))el.textContent=t(el.dataset.t);
    if(simulation)document.querySelector('[data-t="prepare"]').textContent=language==='zh'?'这里只连接合成设备。下方两项仅确认虚构测试条件，不要操作真实微雪或 TF。':'Only a synthetic device is connected. The two checks below confirm fictional test conditions. Do not operate your real board or TF.';
    $('build-notice').textContent=t(simulation?'mock':recoveryEnabled?'candidate':'locked');
    $('build-id').textContent=`${build} · ${sourceCommit.slice(0,12)}`;
    $('environment').textContent=t(supported?'supported':'unsupported');
    $('mode-restore').hidden=!recoveryEnabled;
    for(const key of ['backup','restore','verify']){$(`mode-${key}`).setAttribute('aria-pressed',String(mode===key));$(`mode-${key}`).disabled=busy;}
    $('package-section').hidden=mode==='backup';$('package-file').disabled=busy;
    $('connect-section').hidden=!!session||closed;
    $('connect').disabled=busy||!supported||!$('board-confirmed').checked||!$('tf-removed').checked||(mode!=='backup'&&!packageBytes);
    $('backup-section').hidden=mode!=='backup'||!session;
    $('raw-file').disabled=busy||session?.state!=='DEVICE_VERIFIED';$('capture').disabled=busy||session?.state!=='DEVICE_VERIFIED';
    $('save-package').disabled=busy||!downloadBytes;
    $('saved-file').disabled=busy||!session||!['DEVICE_VERIFIED','SAVED_FILE_REIMPORT_REQUIRED'].includes(session.state);
    $('restore-section').hidden=mode!=='restore'||!session||closed;
    $('restore').disabled=busy||!recoveryEnabled||session?.state!=='READY_FOR_CONFIRMATION'||!$('restore-consent').checked;
    $('verify-section').hidden=mode!=='verify'||!session||closed;$('verify').disabled=busy||session?.state!=='DEVICE_VERIFIED';
    const stage=diagnostics.snapshot().stage;
    $('status').textContent=t(notice||stage);$('error').hidden=!error;$('error').textContent=error;
    $('package-summary').textContent=summary?`${t('selected')} · ${summary.createdAt} · ${summary.firmware} · 16 MiB`:'';
    $('saved-result').textContent=session?.state==='BACKUP_SAVED_VERIFIED'?t('saved'):'';
    $('boot-section').hidden=stage!=='VERIFIED_BOOT_PENDING';
    $('offline-help').textContent=t(originType==='FILE'?'offlineLocal':recoveryEnabled?'offlineHelp':'offlineLocked');
    $('offline-download').hidden=originType==='FILE';
    document.querySelector('[data-t="guide"]').href=embeddedGuide?'#offline-guide':`docs/RECOVERY_GUIDE.${language==='zh'?'zh-CN':'en'}.md`;
    if(offlineHref)$('offline-download').href=offlineHref;
    $('diagnostics').textContent=JSON.stringify(receipt(),null,2);
  }
  function clearPrivateUI() {
    packageBytes?.fill(0);downloadBytes?.fill(0);packageBytes=downloadBytes=undefined;summary=undefined;
    for(const id of ['package-file','raw-file','saved-file'])$(id).value='';
    for(const id of ['board-confirmed','tf-removed','restore-consent'])$(id).checked=false;
    for(const url of objectUrls)URL.revokeObjectURL(url);objectUrls.clear();
    $('package-summary').textContent='';$('saved-result').textContent='';
  }
  async function closeSession() {
    let failure;try{await session?.close();}catch(e){failure=e;}
    finally{session=undefined;clearPrivateUI();closed=true;diagnostics.check('cleanup',{sessionPrivateReferencesCleared:true,domIdentityCleared:true,fileInputsCleared:true,objectUrlsRevoked:true,physicalMemoryErasureGuaranteed:false});}
    if(failure){error=t('errorClose');throw failure;}
  }
  function download(bytes,name,type) {
    const url=URL.createObjectURL(new Blob([bytes],{type}));objectUrls.add(url);const a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();
    setTimeout(()=>{URL.revokeObjectURL(url);objectUrls.delete(url);},30000);
  }
  async function run(fn,{fileOnly=false}={}) {
    if(busy)return;busy=true;notice='';error='';render();const ticker=setInterval(render,300);
    activeTask=(async()=>{
      try{await fn();}
      catch(e){
        const code=mapError?mapError(e):(/^[A-Z_0-9]{1,48}$/.test(e?.code||'')?e.code:e?.name==='NotFoundError'?'PORT_SELECTION_CANCELLED':'IO');diagnostics.error({code});
        const chooserCancelled=code==='PORT_SELECTION_CANCELLED';
        const recoverable=(fileOnly&&session?.state!=='STOP')||chooserCancelled;
        error=`${t(code==='BACKUP_STALE'&&session?.state==='DEVICE_VERIFIED'?'errorStale':recoverable?'errorFile':'errorStop')} (${code})`;
        if(chooserCancelled){notice=mode==='backup'?'readyBackup':'choose';error=language==='zh'?'已取消串口选择，尚未连接；可以重新选择。':'Port selection cancelled; no connection. You may choose again.';}
        if(!recoverable){session?.cancel(code);try{await closeSession();}catch{}diagnostics.stage('STOP');}
        if(errorHelp?.(code,language))error+=` ${errorHelp(code,language)}`;
        if(receipt().flashOutcome==='POSSIBLY_PARTIAL')error+=` ${t('partial')}`;
        else error+= language==='zh'?' 尚未擦除或写入，原机 Flash 内容未被本次操作修改。':' No erase or write has started. This operation has not modified the original Flash.';
      }finally{clearInterval(ticker);busy=false;render();}
    })();return activeTask;
  }
  async function resetMode(next) {
    if(busy)return;await closeSession();mode=next;diagnostics=newDiagnostics();diagnostics.check('writeLock',!recoveryEnabled||mode!=='restore');closed=false;notice=mode==='backup'?'readyBackup':'choose';error='';render();
  }
  for(const next of ['backup','restore','verify'])$(`mode-${next}`).addEventListener('click',()=>{if(next==='restore'&&!recoveryEnabled)return;resetMode(next).catch(()=>render());});
  $('language').addEventListener('click',()=>{language=language==='zh'?'en':'zh';render();});
  for(const id of ['board-confirmed','tf-removed','restore-consent'])$(id).addEventListener('change',render);
  $('package-file').addEventListener('change',()=>{
    const file=$('package-file').files[0];if(!file)return;
    run(async()=>{
      await closeSession();diagnostics=newDiagnostics();diagnostics.check('writeLock',!recoveryEnabled||mode!=='restore');closed=false;
      requireSafe(file.size>16777216&&file.size<=MAX_PACKAGE_BYTES,'PACKAGE_SIZE');
      const bytes=new Uint8Array(await file.arrayBuffer());let handle;
      try{handle=await validateRecoveryPackage(bytes);summary=recoverySummary(handle);packageBytes=bytes;diagnostics.check('packageValidated',true);diagnostics.stage('PACKAGE_VALIDATED');}
      catch(e){bytes.fill(0);throw e;}finally{destroyRecoveryPackage(handle);$('package-file').value='';}
    },{fileOnly:true});
  });
  $('connect').addEventListener('click',()=>{
    if(busy||$('connect').disabled)return;
    // requestDriver invokes requestPort BEFORE the first await: actual user gesture.
    const pending=requestDriver(mode,diagnostics);
    run(async()=>{
      const driver=await pending;session=new RecoverySession(driver,{mode,diagnostics,build,sourceCommit});
      if(mode!=='backup')await session.selectPackage(packageBytes);
      try{await session.prepare({boardConfirmed:$('board-confirmed').checked,tfRemoved:$('tf-removed').checked});}
      finally{if(diagnostics.snapshot().events.openSuccess>0){if(seenOpened)diagnostics.event('manualReconnects');seenOpened=true;}}
      packageBytes?.fill(0);packageBytes=undefined;
    });
  });
  $('raw-file').addEventListener('change',()=>{
    const file=$('raw-file').files[0];if(!file)return;
    run(async()=>{let b;try{requireSafe(file.size===16777216,'PACKAGE_SIZE');b=new Uint8Array(await file.arrayBuffer());downloadBytes=await session.useExistingRaw(b);}finally{b?.fill(0);$('raw-file').value='';}},{fileOnly:true});
  });
  const progress=value=>{$('progress').hidden=false;$('progress').value=value;};
  $('capture').addEventListener('click',()=>run(async()=>{downloadBytes=await session.capture({progress});}));
  $('save-package').addEventListener('click',()=>{
    if(!downloadBytes||busy)return;
    const name=`Draftor-${simulation?'SYNTHETIC-TEST':'private'}-recovery-${new Date().toISOString().replace(/[:.]/g,'-')}-${crypto.randomUUID().slice(0,8)}.zip`;
    if(typeof window.showSaveFilePicker!=='function'){download(downloadBytes,name,'application/zip');return;}
    // A normal user-gesture save dialog; successful saving still does NOT satisfy
    // the separate saved-file reimport / current full-ROM-digest requirement.
    const chosen=window.showSaveFilePicker({suggestedName:name,types:[{description:'Draftor private recovery ZIP',accept:{'application/zip':['.zip']}}]});
    run(async()=>{
      let writable;
      try{const handle=await chosen;writable=await handle.createWritable();await writable.write(downloadBytes);await writable.close();writable=undefined;}
      catch(e){if(e?.name==='AbortError'){notice='SAVED_FILE_REIMPORT_REQUIRED';return;}throw e;}
      finally{if(writable)await writable.abort().catch(()=>{});}
    },{fileOnly:true});
  });
  $('saved-file').addEventListener('change',()=>{
    const file=$('saved-file').files[0];if(!file)return;
    run(async()=>{let b;try{requireSafe(file.size>16777216&&file.size<=MAX_PACKAGE_BYTES,'PACKAGE_SIZE');b=new Uint8Array(await file.arrayBuffer());await session.verifySavedPackage(b);}finally{b?.fill(0);$('saved-file').value='';}},{fileOnly:true});
  });
  $('restore').addEventListener('click',()=>{if($('restore').disabled)return;run(()=>session.restore({restoreConfirmed:$('restore-consent').checked,progress}));});
  $('verify').addEventListener('click',()=>{if($('verify').disabled)return;run(()=>session.verifyAgain());});
  $('close').addEventListener('click',async()=>{
    if(busy){session?.cancel();await activeTask;}
    try{await closeSession();notice='closed';}catch{notice='STOP';}render();
  });
  $('save-receipt').addEventListener('click',()=>{const b=new TextEncoder().encode(JSON.stringify(receipt(),null,2)+'\n');download(b,'Draftor-R9-sanitized-receipt.json','application/json');});
  addEventListener('beforeunload',e=>{if(busy||session){e.preventDefault();e.returnValue='';}});
  addEventListener('pagehide',()=>{session?.cancel();session?.close().catch(()=>{});clearPrivateUI();});
  notice=mode==='backup'?'readyBackup':'choose';render();
}
