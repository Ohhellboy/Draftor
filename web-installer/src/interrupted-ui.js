// SPDX-License-Identifier: Apache-2.0
import {decodeProtection,reconstructOriginal,verifiedFactory} from './install-protection.js';
import {loadRelease} from './firmware.js';import {Diagnostics} from './diagnostics.js';import {requireSafe} from './safety.js';
export function startInterruptedUI({requestDriver,build,sourceCommit,reviewOnly=false}){
 const $=id=>document.getElementById(id);let point,driver,busy=false,stopped=false,zip,diag;
 const say=text=>{$('status').textContent=text;};
 const alive=()=>requireSafe(!stopped,'CANCELLED');
 async function close(){try{await driver?.disconnect();}finally{driver=undefined;point?.fill(0);point=undefined;$('point-file').value='';diag?.check('cleanup',{sessionPrivateReferencesCleared:true,fileInputsCleared:true,physicalMemoryErasureGuaranteed:false});}}
 $('point-file').addEventListener('change',async()=>{if(busy)return;zip?.fill(0);zip=undefined;$('download-original').disabled=true;point?.fill(0);point=undefined;const f=$('point-file').files[0];if(!f)return;try{requireSafe(f.size<70000,'PROTECTION_SIZE');const b=new Uint8Array(await f.arrayBuffer());const p=await decodeProtection(b);point=b;stopped=false;$('prepare-original').disabled=reviewOnly;say(`保护文件已核对 / Protection file checked. ${p.createdAt} · ${p.mode==='fresh'?'厂家原系统 / factory original':'原 Alpha 3 系统 / original Alpha 3'}`);}catch{say('保护文件无效，未访问设备 / Invalid protection file; no device access.');}});
 $('prepare-original').addEventListener('click',async()=>{if(busy||!point||reviewOnly)return;busy=true;$('prepare-original').disabled=true;$('point-file').disabled=true;
  try{diag=new Diagnostics({build,sourceCommit,mode:'reconstruction-read-only',simulation:build.includes('SOFTWARE')});driver=await requestDriver(diag);alive();await driver.connect();say('只读核对原板并准备原系统文件… / Read-only original-device check and reconstruction…');
   const release=await loadRelease(new URL('../firmware/',import.meta.url)),r=await fetch(new URL('../factory/FactoryOnly.bin',import.meta.url));requireSafe(r.ok,'FACTORY_ASSET');const factory=await verifiedFactory(new Uint8Array(await r.arrayBuffer()));
   try{zip=await reconstructOriginal(driver,point,release,{factory,boardConfirmed:true,tfRemoved:true,build,sourceCommit,alive,progress:p=>say(`正在只读准备 / Reading: ${Math.round(p*100)}%`)});}finally{factory.fill(0);}
   alive();await close();$('download-original').disabled=false;say('原系统文件已重建并完整校验，串口已关闭，尚未写入。保存 ZIP 后，下载 R12 离线工具，选择“恢复原设备”并导入这个 ZIP；再次确认整片恢复范围。\nOriginal image reconstructed and verified; port closed, no write. Save the ZIP and R12 offline tool, then choose Restore original device and import this ZIP. Full restore requires separate confirmation.');
  }catch(e){stopped=true;driver?.cancel();await close();say(`${e.code||'IO'} · 已停止，不重试。私人区域变化或原板不匹配时不能恢复。 / Stopped, no retry. Changed private regions or a different board cannot be restored.`);}finally{busy=false;$('point-file').disabled=false;}
 });
 $('download-original').addEventListener('click',()=>{if(!zip)return;const u=URL.createObjectURL(new Blob([zip],{type:'application/zip'})),a=document.createElement('a');a.href=u;a.download='Draftor-R12-reconstructed-original.zip';a.click();setTimeout(()=>URL.revokeObjectURL(u),30000);});
 $('close-point').addEventListener('click',async()=>{stopped=true;driver?.cancel();if(!busy)await close();zip?.fill(0);zip=undefined;$('download-original').disabled=true;$('prepare-original').disabled=true;say('已停止；请保留原保护文件。 / Stopped; keep the original protection file.');});
 addEventListener('beforeunload',e=>{if(busy||driver){e.preventDefault();e.returnValue='';}});addEventListener('pagehide',()=>{stopped=true;driver?.cancel();driver?.disconnect().catch(()=>{});point?.fill(0);zip?.fill(0);});
}
