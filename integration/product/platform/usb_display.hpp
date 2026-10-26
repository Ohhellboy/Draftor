#pragma once
#include "usb_transfer.hpp"
#include "secondary_menu.hpp"
#include <cstdio>
#include <cstring>

namespace s3app {
namespace usb_display {
// This dictionary translates controller messages without substituting a phase,
// result, operation ID, or transaction. Unknown future messages remain visible.
inline const char* message(const char* original) noexcept {
 struct Entry{const char* original;const char* chinese;};
 static constexpr Entry entries[]={
  {"Choose one transfer","选择传输方式"},
  {"Preparing Inbox","正在准备接收盘"},
  {"Saving current version","正在保存当前版本"},
  {"Closing safely...","正在安全结束…"},
  {"Unconfirmed receipt; copy again","接收未经确认，请重新复制"},
  {"TXT could not be opened","无法打开 TXT 文件"},
  {"Import rejected; original retained","导入未获准，原文档保留"},
  {"Validating TXT / creating new draft","正在验证 TXT 并创建新文档"},
  {"Close stalled; resources retained","安全结束受阻，资源仍保留"},
  {"Cancelled after publish; draft retained","发布后已取消，新文档保留"},
  {"Publication uncertain; check operation","发布结果待确认，请核查操作号"},
  {"Saving original before opening","打开前正在保存原文档"},
  {"Save failed; original retained","保存失败，原文档保留"},
  {"USB startup failed; original retained","USB 启动失败，原文档保留"},
  {"USB unavailable; reconnect data cable","USB 不可用，请重新连接数据线"},
  {"Copy TXT to Inbox, then eject","复制 TXT 到接收盘后正常弹出"},
  {"Inbox allocation failed","接收盘空间申请失败"},
  {"Preparing immutable DOCUMENT.TXT","正在准备只读 DOCUMENT.TXT"},
  {"Export admission failed","导出未获准"},
  {"Ejected; original retained","已正常弹出，原文档保留"},
  {"Disconnected; copy not confirmed","连接已断开，复制未经确认"},
  {"Export failed; original retained","导出失败，原文档保留"},
  {"Inbox failed; original retained","接收盘失败，原文档保留"},
  {"Unconfirmed receipt; no draft created","接收未经确认，未创建文档"},
  {"Received; choose TXT to create new","已接收，选择 TXT 创建新文档"},
  {"Too many TXT; maximum 32 per session","TXT 过多，每次最多 32 个"},
  {"No visible TXT in Inbox","接收盘中没有可见 TXT"},
  {"Inbox directory invalid","接收盘目录无效"},
  {"New draft published; original retained","新文档已发布，原文档保留"},
  {"Publication uncertain; keep operation","创建结果待确认，请保留操作号"},
  {"Import failed; original retained","导入失败，原文档保留"},
  {"Open failed; published draft retained","打开失败，已发布的文档保留"},
  {"Open save failed; new draft retained","打开前保存失败，新文档保留"},
  {"Opened; CURRENT update uncertain","已打开，当前文档记录更新待确认"},
  {"Opened; memory low / state uncertain","已打开，内存不足或状态待确认"},
  {"Memory low; check publication receipt","内存不足，请核查发布回执"},
  {"Memory low; original retained","内存不足，原文档保留"},
  {"Recovered published draft; original retained","已找回发布结果，原文档保留"}
 };
 for(const auto& e:entries)if(std::strcmp(original,e.original)==0)return e.chinese;
 return original;
}
inline const char* status(usb::Status value) noexcept {
 using S=usb::Status;
 switch(value){
  case S::Ok:return "正常";case S::Busy:return "忙碌";case S::NotReady:return "尚未就绪";
  case S::InvalidArgument:return "参数无效";case S::Bounds:return "范围错误";
  case S::ReadOnly:return "只读";case S::Stale:return "状态已过期";case S::Io:return "读写失败";
  case S::Full:return "空间不足";case S::ShortRead:return "读取不完整";case S::ShortWrite:return "写入不完整";
  case S::SyncFailed:return "同步失败";case S::Filesystem:return "文件系统错误";case S::TooLarge:return "文件过大";
  case S::InvalidText:return "文本无效";case S::Cancelled:return "已取消";case S::Contract:return "接口状态错误";
  case S::Exhausted:return "资源耗尽";case S::Conflict:return "状态冲突";case S::NotFound:return "未找到";
  case S::Uncertain:return "结果待确认";
 }
 return "未知状态";
}
}
// Read cached transfer state only. USB owns the exact same candidate list,
// selection, receipt, commands, and safe-close protocol as Alpha 2.
template<class Fill,class Aux,class Content>
void paintUsbPanel(const UsbTransfer& transfer,const s3host::FontPort& contentFont,
                  Fill fill,Aux aux,Content content){
 using namespace display;
 const auto phase=transfer.phase();const auto receipt=transfer.imported();
 secondary::header(phase==UsbUiPhase::Menu?"USB 传输":transfer.importing()?"USB 导入":"USB 导出",fill,aux);
 if(phase==UsbUiPhase::Selecting){
  aux("接收盘已正常弹出",8,40,false);
  const auto selected=transfer.selectedCandidate();const auto first=selected>=4?selected-3:0;
  for(std::size_t i=first;i<transfer.candidateCount()&&i<first+4;++i){
   const int y=84+int(i-first)*28;if(i==selected)secondary::triangle(y,fill);
   secondary::preview(transfer.candidateName(i),contentFont,28,y,264,content);
  }
  aux(transfer.candidatesReady()?"选择 TXT，新建文档。":"正在扫描接收到的文件…",8,232,false);
  aux("电脑原文件保持不变。",8,256,false);
  aux("上下选择 Enter创建 Esc安全结束",8,372,false);return;
 }
 if(phase==UsbUiPhase::Exporting){
  aux(transfer.exportPublished()?"在 Finder 中复制":"正在保存和准备…",8,64,false);
  content("DOCUMENT.TXT",8,100,false);
  aux("只读：不能改名或删除。",8,156,false);
  aux("复制完成后，在 Finder 弹出。",8,180,false);
  aux("Esc：安全结束",8,372,false);return;
 }
 if(phase==UsbUiPhase::Result&&receipt.status==usb::Status::Uncertain&&receipt.operation&&
    receipt.outcome!=usb::Outcome::Imported&&receipt.outcome!=usb::Outcome::Opened){
  content("创建结果待确认",8,76,false);
  char number[32];std::snprintf(number,sizeof(number),"%llu",static_cast<unsigned long long>(receipt.operation));
  aux("操作号",8,122,false);content(number,88,120,false);
  aux("请保留操作号，查询同一次结果。",8,176,false);
  aux("原文档及电脑原文件保持不变。",8,200,false);
  aux("R：查询结果  Esc：安全结束",8,372,false);return;
 }
 // Measuring auxiliary text with the larger content face is conservative and
 // keeps all controller messages outside the content area without heap buffers.
 secondary::wrapped(usb_display::message(transfer.displayMessage()),contentFont,8,40,284,24,112,aux);
 if(phase==UsbUiPhase::Menu){
  content("E：导出当前 TXT",8,112,false);content("I：导入为新文档",8,168,false);
 }else if(phase==UsbUiPhase::Receiving){
  aux("在 Finder 拖入 TXT 后正常弹出。",8,128,false);
  aux("接收盘是临时内存空间。",8,176,false);
  content("退出或断电将清空接收盘",8,216,false);
 }else if(phase==UsbUiPhase::Result){
  char number[32];
  if(receipt.outcome==usb::Outcome::Imported){
   aux("已导入字节数",8,128,false);
   std::snprintf(number,sizeof(number),"%llu",static_cast<unsigned long long>(receipt.bodyBytes));
   content(number,8,152,false);
   aux("Enter：打开新文档",8,216,false);
   if(receipt.status==usb::Status::Uncertain&&receipt.operation)aux("R：查询同一次操作结果",8,244,false);
  }else{
   aux("状态",8,128,false);aux(usb_display::status(transfer.status()),76,128,false);
   if(receipt.operation){
    aux("操作号",8,170,false);
    std::snprintf(number,sizeof(number),"%llu",static_cast<unsigned long long>(receipt.operation));
    content(number,8,196,false);
   }
   if(receipt.status==usb::Status::Uncertain&&receipt.operation)
    aux("R：查询同一次操作结果",8,244,false);
  }
  aux("电脑原文件保持不变。",8,292,false);
 }
 const char* footer="Esc：安全结束";
 if(phase==UsbUiPhase::Menu)footer="Esc：返回原页面";
 else if(phase==UsbUiPhase::Closing)footer="请等待安全结束";
 else if(receipt.outcome==usb::Outcome::Opened)footer="Esc：继续已打开的文档";
 aux(footer,8,372,false);
}
}
