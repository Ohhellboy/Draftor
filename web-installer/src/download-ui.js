// SPDX-License-Identifier: Apache-2.0
import {ReleaseDownloader} from './firmware.js';
const copy={
 zh:{preparing:'正在准备安装文件',checking:'正在检查安装文件',failed:'下载失败，请检查网络后重试',retry:'重新下载',details:'下载详情（不含个人数据）',receiving:'正在下载',file:'文件',idle:'连接长时间没有收到新数据。',network:'网络连接失败或下载中断。',http:'服务器暂时无法提供文件。',incomplete:'文件未下载完整。',invalid:'文件校验未通过，不能用于安装。',safe:'尚未连接设备，未擦除或写入。',cancelled:'已取消下载，未擦除或写入。'},
 en:{preparing:'Preparing installation files',checking:'Checking installation files',failed:'Download failed. Check your network and retry.',retry:'Retry download',details:'Download details (no personal data)',receiving:'Downloading',file:'File',idle:'No new data arrived for too long.',network:'The network connection failed or the download was interrupted.',http:'The server could not provide the file.',incomplete:'The file was not downloaded completely.',invalid:'File verification failed. It cannot be used for installation.',safe:'No device connection, erase or write has started.',cancelled:'Download cancelled. No erase or write has started.'}
};
const toolCopy={
 zh:{preparing:'正在下载离线恢复工具',checking:'正在检查离线恢复工具',failed:'离线恢复工具下载失败，请检查网络后重试',retry:'重新下载离线恢复工具',safe:'尚未擦除或写入，原机 Flash 内容未被本次操作修改。',cancelled:'已取消离线恢复工具下载，尚未擦除或写入。'},
 en:{preparing:'Downloading the offline recovery tool',checking:'Checking the offline recovery tool',failed:'Offline recovery tool download failed. Check your network and retry.',retry:'Retry offline recovery tool download',safe:'No erase or write has started. This operation has not changed the original Flash.',cancelled:'Offline tool download cancelled. No erase or write has started.'}
};
const amount=n=>(n/1000000).toFixed(2)+' MB';
// This controller only prepares public files. It cannot request a serial port or
// call a device session; retry retains this same loader's verified-only cache.
export function downloadPanel({anchor,onChange,onRetry,options={},prefix='download',purpose='installation',createLoader=opts=>new ReleaseDownloader(opts)}){
 const section=document.createElement('section');section.id=prefix+'-preparation';section.hidden=true;
 section.innerHTML='<p id="download-progress-text" aria-live="polite"></p><progress id="download-progress" max="1" value="0"></progress><p id="download-problem" role="alert"></p><button id="retry-download" type="button"></button><details id="download-details"><summary></summary><pre></pre></details>';
 for(const element of section.querySelectorAll('[id]'))element.id=element.id==='retry-download'?'retry-'+prefix:prefix+element.id.slice('download'.length);
 anchor.after(section);
 const $=id=>section.querySelector(id==='retry-download'?'#retry-'+prefix:'#'+prefix+id.slice('download'.length));let loader,state={stage:'IDLE'},lang='zh';
 $('retry-download').addEventListener('click',()=>{if(state.stage==='FAILED')onRetry();});
 function text(language){return purpose==='offline-tool'?{...copy[language],...toolCopy[language]}:copy[language];}
 function status(language){const c=text(language);return state.stage==='FAILED'?c.failed:state.stage==='CANCELLED'?c.cancelled:['VERIFYING','VERIFYING_RELEASE'].includes(state.stage)?c.checking:c.preparing;}
 function render(language,visible,{canRetry=true}={}){
  lang=language;const c=text(lang);section.hidden=!visible;
  const failed=state.stage==='FAILED',active=['CONNECTING','RECEIVING'].includes(state.stage);
  const detail=state.error,category=detail?.category;
  $('download-progress-text').textContent=active?`${c.file} ${state.fileIndex} / ${state.fileCount} · ${c.receiving}：${amount(state.receivedBytes||0)}${Number.isFinite(state.expectedBytes)?' / '+amount(state.expectedBytes):''}`:'';
  $('download-progress').hidden=!active;
  if(Number.isFinite(state.expectedBytes)){$('download-progress').max=state.expectedBytes;$('download-progress').value=state.receivedBytes||0;}else $('download-progress').removeAttribute('value');
  $('download-problem').hidden=!failed;
  $('download-problem').textContent=failed?(category==='IDLE_TIMEOUT'?c.idle:category==='HTTP_STATUS'?c.http:category==='INCOMPLETE'?c.incomplete:['LENGTH','SHA256','MD5','RELEASE'].includes(category)?c.invalid:c.network)+' '+c.safe:'';
  $('retry-download').hidden=!failed;$('retry-download').disabled=!failed||!canRetry;$('retry-download').textContent=c.retry;
  $('download-details').querySelector('summary').textContent=c.details;
  // Never expose URLs, browser exception messages, file contents or device data.
  $('download-details').querySelector('pre').textContent=JSON.stringify(state,null,2);
 }
 return {
  reset(){loader?.dispose();state={stage:'IDLE'};loader=createLoader({...options,onProgress:value=>{state=value;onChange();}});},
  load(){return loader.load();},extraFile(name){return loader.extraFile(name);},
  cancel(){loader?.cancel();},dispose(){loader?.dispose();},
  snapshot(){return structuredClone(state);},status,render
 };
}
