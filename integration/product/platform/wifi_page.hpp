// SPDX-License-Identifier: Apache-2.0
#pragma once
namespace s3app {
inline constexpr char WifiPage[]=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Draftor 文档传输</title>
<style>[hidden]{display:none!important}body{font:17px system-ui;max-width:36rem;margin:2rem auto;padding:0 1.2rem;background:#f5f3ee;color:#242724}h1{font-size:1.8rem}section{background:white;padding:1.3rem;border-radius:16px;margin:1rem 0}button,a{display:inline-block;padding:.8rem 1rem;border:0;border-radius:8px;background:#234d45;color:white;font:inherit;text-decoration:none}input{max-width:100%;font:inherit}p{line-height:1.6}#result{white-space:pre-wrap;overflow-wrap:anywhere}small{color:#555}</style>
<h1>Draftor 文档传输</h1><p>本地连接 · 无需互联网</p>
<section><h2>导出当前文档</h2><p>下载开始时的 TXT 快照。下载期间可继续写作。</p><a id="download" hidden>下载 DOCUMENT.TXT</a></section>
<section><h2>导入新文档</h2><p>UTF-8 TXT，正文最多 65,536 字节。导入不会覆盖或打开当前文档。</p><input id="file" type="file" accept=".txt,text/plain"><p id="detail">未选择文件</p><button id="upload" disabled>创建新文档</button><p><small>导入时草稿机会短暂暂停编辑，完成后继续。</small></p></section>
<p id="result" role="status" aria-live="polite">正在连接…</p><button id="recover" hidden>查询上次导入结果</button>
<p><small>完成后在草稿机按 F10，再按 O 关闭 Wi-Fi。热点信息页按 Esc 返回写作，热点保持开启。iPhone 提示“无互联网”时请保持连接。</small></p>
<script>
let token='',busy=false;const $=id=>document.getElementById(id),say=t=>$('result').textContent=t;
const pendingKey='draftor.pendingImport';let pending='';try{pending=localStorage.getItem(pendingKey)||''}catch(e){}
function remember(op){pending=op;try{op?localStorage.setItem(pendingKey,op):localStorage.removeItem(pendingKey)}catch(e){}$('recover').hidden=!op}
async function api(path,options={}){let r=await fetch(path,{cache:'no-store',...options});let j=await r.json();if(!r.ok)throw Error(j.message||('HTTP '+r.status));return j}
async function recover(){if(!pending)return;try{let j=await api('/receipt?token='+token+'&op='+pending);say(j.message+'\n操作号 '+pending+'\n文档 '+(j.document||''));if(j.imported||j.retrySafe)remember('')}catch(e){say('请保留操作号 '+pending+'：'+e.message+'。未确认前不要重复上传。')}}
$('recover').onclick=recover;
$('file').onchange=()=>{const f=$('file').files[0];$('detail').textContent=f?f.name+' · '+f.size+' bytes':'未选择文件';$('upload').disabled=busy||!f||!token};
$('upload').onclick=async()=>{const f=$('file').files[0];if(!f||busy)return;if(pending){say('请先查询上次导入结果，避免重复创建。');return}if(!/\.txt$/i.test(f.name)||f.size>131075){say('请选择 TXT。原始文件最多 131,075 字节，规范化后正文最多 65,536 字节。');return}busy=true;$('upload').disabled=true;try{const t=await api('/ticket',{method:'POST',headers:{'X-Draftor-Token':token},body:''});remember(t.operation);say('正在导入 '+f.name+'…');const j=await api('/import?op='+pending+'&name='+encodeURIComponent(f.name),{method:'POST',headers:{'Content-Type':'application/octet-stream','X-Draftor-Token':token},body:f});say(j.message+'\n'+j.bytes+' bytes · 文档 '+j.document);if(j.imported||j.retrySafe)remember('')}catch(e){say(e.message+'\n若连接中断，请查询操作号 '+pending+' 的结果。');$('recover').hidden=!pending}finally{busy=false;$('upload').disabled=false}};
api('/session').then(j=>{token=j.token;$('download').href='/export?token='+token;$('download').hidden=false;$('upload').disabled=!$('file').files[0];say('已连接 Draftor。');$('recover').hidden=!pending;if(pending)recover()}).catch(e=>say('连接失败：'+e.message+'。请重新连接热点后刷新。'));
</script></html>)HTML";
}
