#include "cp_observe.hpp"
#if DRAFTOR_CP_OBSERVE && defined(DRAFTOR_BLOCK_BACKEND) && DRAFTOR_BLOCK_BACKEND
#include "base_order_internal.hpp"
#endif
#include "horizontal_body.hpp"
#include "stability_diagnostics.hpp"
#include "behavior_bench.hpp"
#include "behavior_observer.hpp"
#if DRAFTOR_TELEMETRY_PROFILE
#include <draftor/telemetry/SerialTelemetrySink.h>
#endif
#if DRAFTOR_TELEMETRY_CAPTURE
#include <esp_log.h>
#endif
#include "body_caret.hpp"
#include "order_allocation.hpp"
#include "candidate_build.hpp"
#include "docmanager_display.hpp"
#include "device_main.hpp"
#include "service_endpoints.hpp"
#include "operation_profile.hpp"
#include "input_route.hpp"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include "ST7306_OsptekLpm8.h"
#include "writing_app.hpp"
#include "hanime_document_sd_storage.hpp"
#include "ble_keyboard_host.hpp"
#include "chinese_font.hpp"
#include "tf_resources.hpp"
#include "device_ime.hpp"
#include "raw_viewport.hpp"
#include "horizontal_raw.hpp"
#include "loading_status.hpp"
#include "io_profile.hpp"
#include "tf_probe.hpp"
#include "tf_mount_recovery.hpp"
#include "ui_trace.hpp"
#include "idle_log.hpp"
#include "display_schedule.hpp"
#include "display_tx_observe.hpp"
#include "ime_presentation.hpp"
#include "vertical_view.hpp"
#include "product_input.hpp"
#include "metadata_font.hpp"
#include "menu_font.hpp"
#ifndef DRAFTOR_USB_ENABLED
#define DRAFTOR_USB_ENABLED 1
#endif
#if DRAFTOR_USB_ENABLED
#include "usb_platform_s3.hpp"
#include "usb_runtime_allocation.hpp"
#include "wifi_platform_s3.hpp"
#include "platform_memory.hpp"
#include "wifi_display.hpp"
#include "usb_display.hpp"
#endif

#include "presentation_frame.hpp"
#include "vertical_hud.hpp"
#include "editor_hud.hpp"
#include <ffconf.h>
#include <esp_idf_version.h>
#include <cstdarg>
#include <hanime_mount_budget.h>
#include "selection_highlight.hpp"
#include "find_highlight.hpp"
#include "reader_display.hpp"
#include <algorithm>
#include <cstring>
extern "C" const uint32_t s3_ime_memory_abi[];
extern "C" const uint32_t s3_ime_compile_abi[];
extern "C" const uint32_t s3_next_stage_abi[];
extern "C" const uint32_t s3_lc1_capacity_abi[];
extern "C" const uint32_t s3_mem1_abi[];
extern "C" __attribute__((used)) const uint32_t s3_memory_abi[] = {
 sizeof(s3app::App),sizeof(editor::Document),sizeof(editor::body_edit::BodyEditor),
 sizeof(hanime::documents::PersistenceService),sizeof(s3hid::Map),sizeof(s3hid::Event),
 editor::body_edit::BodyEditor::historyRecordSize(),sizeof(hanime::documents::DocumentMetadata),
 sizeof(hanime::find::FindSession),sizeof(hanime::text_stats::Counter),
 sizeof(hanime::find::integration::HanimeDocumentAdapter),sizeof(hanime::find::input::FindController)
};
#ifndef HANIME_TF_RUNTIME_HZ
#define HANIME_TF_RUNTIME_HZ 20000000
#endif
static_assert(HANIME_TF_RUNTIME_HZ==20000000,"T06 TF20 candidate requires 20 MHz runtime; device qualification pending");
namespace {
#if FF_USE_FASTSEEK
constexpr unsigned FastSeekCompiled=1;
#else
constexpr unsigned FastSeekCompiled=0;
#endif
constexpr const char* Firmware=ime_device::CandidateFirmware;

SPIClass displaySpi(FSPI),tfSpi(HSPI);
ST7306OsptekLpm8 lcd(displaySpi,12,11,10,9,4); // donor framebuffer remains internal RAM
s3fs::Fs filesystem("/sd");
hanime::documents::ArduinoSdStoragePort storage(filesystem);
ime_device::ChineseFont font;
ime_device::Resources* imeResources=nullptr;
s3host::BuiltinSmallFont smallFont;
s3app::MenuFont14 menuFont14;
s3app::MenuFont16 menuFont16;
s3app::App* app=nullptr;
ime_device::DeviceIme* ime=nullptr;
uint32_t lastSaveTurn=0;
ime_device::LoadingStatus loadingStatus;
ime_device::DisplaySchedule displaySchedule;
struct InitScheduling {uint64_t turns=0,slices=0,blocked_by_keys=0,save_turns=0,save_deferred=0;uint32_t max_slice_us=0,max_turn_us=0;} initSchedule;
struct ImeTiming {uint64_t request_us=0,first_raw_us=0,last_raw_us=0;uint32_t serial=0,raw_serial=0;bool ready_pending=false,frame_pending=false;} imeTiming;
constexpr uint32_t TfRuntimeHz=HANIME_TF_RUNTIME_HZ;
static_assert(TfRuntimeHz==20000000,"controlled TF20 runtime setting");
// Core requests remain <=1KiB; font/probe uncached reads <=4KiB. CPU limits cannot preempt SD/VFS.
constexpr uint32_t InitQuantumUs=2000,RunQuantumUs=1000;
constexpr unsigned InitSlices=1024,RunSlices=256;
std::unique_ptr<ime_device::TfProbe> tfProbe;
bool rowPrefetchEnabled=false; // A/B diagnostic only; pending-range foreground cost remains open.
bool probeUsedThisBoot=false,resourceErrorReported=false;
uint64_t bootLogUs=0;unsigned diagnosticRow=1000;uint32_t maxDiagnosticUs=0;
char serialCommand[16]{};unsigned serialCommandSize=0;bool serialOverflow=false;
s3hid::Repeat repeat;
const char* fatal=nullptr;
// Heap corruption is distinct from a recoverable allocation failure. Only the
// loop task owns this latch; it never resumes App/IME/storage after detection.
DRAM_ATTR bool heapIntegrityStopped=false;
DRAM_ATTR unsigned char heapStopSent=0;
DRAM_ATTR const char HeapStopMessage[]="\n[ST_STOP] integrity=2 scope=product_loop\n";
static_assert(sizeof(HeapStopMessage)-1<256,"bounded internal stop marker");
__attribute__((noinline)) void heapIntegrityStopTurn(){
 // No App, display framebuffer, PSRAM logger, allocation, mutex or TF access.
 // UART0 was configured at setup; direct FIFO writes are bounded/best effort.
 // Other tasks/interrupts are not halted and can still emit their own output.
 auto* hw=UART_LL_GET_HW(UART_NUM_0);
 unsigned count=unsigned(sizeof(HeapStopMessage)-1)-heapStopSent;
 if(count>16)count=16;
 const auto room=uart_ll_get_txfifo_len(hw);if(count>room)count=room;
 if(count){uart_ll_write_txfifo(hw,reinterpret_cast<const uint8_t*>(HeapStopMessage)+heapStopSent,count);heapStopSent+=count;}
 delay(1); // Yield the stopped loop task; do not spin or auto-restart.
}
s3hid::InputRoute inputRoute;
s3app::ProductInput productInput;
#if DRAFTOR_USB_ENABLED
struct UsbRuntime {
 s3app::S3UsbPlatform platform;
 s3app::UsbDocumentImport sink;
 s3app::UsbTransfer transfer;
 UsbRuntime():sink(storage,app->catalog,app->persistence,app->session,app->editorPort),transfer(*app,platform,sink){}
};
// Read from the freshly compiled object for the resource report; no runtime log
// or new telemetry frame is emitted by this compile-time ABI inventory.
extern "C" __attribute__((used)) const volatile uint32_t s3_usb_memory_abi[]={
 sizeof(UsbRuntime),sizeof(s3app::S3UsbPlatform),sizeof(s3app::UsbExportOwner),
 sizeof(s3app::UsbDocumentImport),sizeof(s3app::UsbTransfer),512*1024,65536,131075};
UsbRuntime* usbRuntime=nullptr;
bool usbActive(){return usbRuntime&&usbRuntime->transfer.active();}
s3app::TransferInputState usbInput(){return {ime&&ime->active(),ime&&ime->hasWork(),false};}
bool usbKey(const s3hid::Event& e){
 if(!usbRuntime){
  if(!s3app::usbEntryKey(*app,e))return false;
  if(!s3app::stability::capture(s3app::stability::Kind::UsbBefore,e.usage,e.epoch,0,true)){heapIntegrityStopped=true;return true;}
  if(!s3app::ensureUsbRuntime(usbRuntime)){s3app::stability::capture(s3app::stability::Kind::UsbDenied,sizeof(UsbRuntime));app->notice="USB unavailable: memory low";app->dirtyDisplay=true;return true;}
  s3app::stability::capture(s3app::stability::Kind::UsbReady,sizeof(UsbRuntime));
 }
 const auto before=usbRuntime->transfer.phase();
 const bool consumed=usbRuntime->transfer.key(e,usbInput());
 if(before!=usbRuntime->transfer.phase())s3app::stability::capture(s3app::stability::Kind::UsbPhase,unsigned(before),unsigned(usbRuntime->transfer.phase()));
 return consumed;
}
#else
bool usbActive(){return false;}
#endif

#if DRAFTOR_USB_ENABLED
// Wi-Fi reuses the document-transfer module in USB-enabled product builds.
// It is a cold, separately owned runtime. No radio start at boot.
struct WifiRuntime {
 s3app::S3WifiTransport transport;
 s3app::UsbDocumentImport sink;
 s3app::WifiTransfer transfer;
 WifiRuntime():sink(storage,app->catalog,app->persistence,app->session,app->editorPort),transfer(*app,transport,sink){}
};
WifiRuntime* wifiRuntime=nullptr;
extern "C" __attribute__((used)) const volatile uint32_t s3_wifi_memory_abi[]={sizeof(WifiRuntime),sizeof(s3app::WifiTransfer),sizeof(s3app::S3WifiTransport)};
bool wifiBlocked(){return wifiRuntime&&wifiRuntime->transfer.blocksInput();}
void releaseWifiRuntime(){
 if(wifiRuntime&&!wifiRuntime->transfer.active()){
  wifiRuntime->~WifiRuntime();draftor::memory::release(wifiRuntime);wifiRuntime=nullptr;
  s3app::S3WifiTransport measurement;measurement.measure("runtime_freed");
 }
}
bool wifiKey(const s3hid::Event& e){
 if(!wifiRuntime){
  if(!s3app::wifiEntryKey(*app,e))return false;
  if(usbActive()){app->notice="Close USB transfer before Wi-Fi";app->dirtyDisplay=true;return true;}
  if(!s3app::ensureUsbRuntime(wifiRuntime)){app->notice="Wi-Fi unavailable: memory low";app->dirtyDisplay=true;return true;}
 }
 const bool consumed=wifiRuntime->transfer.key(e,usbInput());releaseWifiRuntime();return consumed;
}
#else
bool wifiBlocked(){return false;}
#endif

uint32_t reportedDiscards=0,lastBodyFrameAction=0;
bool lastPairingWarning=false;
uint32_t lastDraw=0,lastMemory=0,lastOperation=0,lastEpoch=0,lastBleState=0,lastPasskey=0;
uint32_t maxLoop=0,maxDraw=0,maxService=0,maxInputAge=0,maxUiFlush=0,uiShortWrites=0;
struct ResponseWindow {uint32_t accepted=0,max_age_ms=0,key_us=0,present_ms=0,save_us=0,log_us=0,pending_ms=0;bool awaiting=false;uint64_t pending_focus=0;} response;

// All queued text and deferred batch state live in one boot-owned PSRAM object.
// Overflow drops diagnostics, never input or document operations.
struct DeferredDiagnostics {
 ime_device::IdleLog output;
 std::array<ime_device::UiTraceRecord,ime_device::UiTrace::Capacity> ui_records{};
 const char* memory_stage=nullptr;
 uint64_t memory_requested_us=0;
 unsigned memory_row=0,summary_row=1000,profile_row=1000,create_row=10;
 uint64_t summary_us=0,operation_us=0,profile_us=0;
 uint32_t superseded=0,operation=0,body_bytes=0,cursor=0;
 uint32_t input_events=0,queue_peak=0,queue_overflow=0,bad_reports=0;
 uint32_t input_age=0,draw_us=0,service_us=0,loop_us=0;
 uint32_t display_sent=0,display_reused=0,display_failed=0,arrow_peak=0,arrow_overflow=0,arrow_discarded=0;
 bool body_committed=false,dirty=false,document_pending=false;
 const char* operation_status="",*profile_status="";
 std::array<char,64> operation_stage{};
 s3app::App::CreateTiming create;
 s3profile::Record profile;
 size_t profile_bytes=0,profile_cursor=0;
 ime_device::Metrics metrics;
 ime_device::IoProfile io;
 ime_device::LoadSnapshot load;
 ime_device::WindowCosts rows;
 InitScheduling schedule;
 ResponseWindow response;
 std::array<hanime::VerifiedResourceSnapshot,3> auth{};
 std::array<hanime::VerifiedResourceMemory,3> auth_memory{};
 ime_device::UiTraceRecord ui;
 unsigned ui_part=ime_device::UiTraceParts;
 uint32_t ui_lost=0,ui_keyups=0,ui_duplicates=0,ui_text_lost=0;
#if DRAFTOR_AUTH_OBSERVE
 // Bounded loss snapshots only; no extra queue or per-service event stream.
 uint64_t observe_log_dropped=0,observe_log_oversized=0;
 uint32_t observe_cp_dropped=0,observe_cp_overwritten=0;
#endif
};
DeferredDiagnostics* diagnostics=nullptr;
extern "C" __attribute__((used)) const volatile uint32_t s3_diagnostic_memory_abi[]={
 sizeof(DeferredDiagnostics),sizeof(ime_device::IdleLog),sizeof(ime_device::UiTrace),
 sizeof(ime_device::UiTraceRecord),ime_device::UiTrace::Capacity,
 ime_device::IdleLog::Capacity,ime_device::IdleLog::LineBytes};
extern "C" __attribute__((used)) const volatile uint32_t s3_perf_memory_abi[]={
 sizeof(ime_device::DisplaySchedule),ST7306OsptekLpm8::BufferSize,
 sizeof(ime_device::IoProfile),sizeof(ime_device::RoleIo),32,FastSeekCompiled};
#if DRAFTOR_TELEMETRY_PROFILE
// Small internal UART staging; the emitter and rich scratch are PSRAM-only.
draftor::telemetry::SerialWriteResult behaviorWrite(void*,const std::uint8_t* bytes,std::size_t size) noexcept {
 const auto n=uart_tx_chars(UART_NUM_0,reinterpret_cast<const char*>(bytes),std::min(size,std::size_t(128)));
 return {n>0?draftor::telemetry::SerialWriteStatus::Progress:draftor::telemetry::SerialWriteStatus::WouldBlock,n>0?std::size_t(n):0};
}
draftor::telemetry::SerialTelemetrySink behaviorSink(behaviorWrite,nullptr);
#endif
ime_device::LoopYield loopYield;
bool detailedDiagnostics=false;
uint32_t logsUnavailable=0;
// The fixed board uses Serial0 (CDCOnBoot=default/0). UART cannot report
// whether a computer is listening. uart_tx_chars only fills available FIFO
// space; unlike HardwareSerial::write it does not wait for room to send a line.
static_assert(!ARDUINO_USB_CDC_ON_BOOT, "This adapter logger consumes the fixed UART0 board configuration");
class SerialLogPort final : public ime_device::LogPort {
public:
 bool connected() const override { return true; }
 size_t available() override { return ime_device::IdleLog::WriteBytesPerTurn; }
 size_t write(const char* data, size_t bytes) override {
  const int written = uart_tx_chars(UART_NUM_0, data, uint32_t(bytes));
  return written > 0 ? size_t(written) : 0;
 }
} serialLogPort;
void logLine(const char* format,...) __attribute__((format(printf,1,2)));
void logLine(const char* format,...){
#if DRAFTOR_TELEMETRY_CAPTURE
#if DRAFTOR_AUTH_OBSERVE
 if(std::strncmp(format,"[IME_LOAD_REQUEST]",18)!=0&&std::strncmp(format,"[IME_READY]",11)!=0)return;
#else
 return;
#endif
#endif

 if(!diagnostics){++logsUnavailable;return;}
 va_list arguments;va_start(arguments,format);
 diagnostics->output.format(format,arguments);
 va_end(arguments);
}
ime_device::LogGate logGate(){
 return {keyboard::pending(),ime&&ime->hasWork(),app&&app->persistence.backgroundSaveActive(),app&&app->commandBarrier()};
}

bool tfMountFailed=false;
void parkTfPins(){pinMode(15,OUTPUT);digitalWrite(15,HIGH);pinMode(17,OUTPUT);digitalWrite(17,HIGH);pinMode(16,OUTPUT);digitalWrite(16,LOW);}
struct BootTfPort {
 uint32_t now()const{return millis();}
 bool unowned()const{return !app&&!imeResources&&!ime&&!font.allocation();}
 void park(){SD.end();tfSpi.end();parkTfPins();}
 void pause(uint32_t ms){delay(ms);}
 s3tf::MountAttempt mount(uint32_t budgetMs){
  const auto start=millis();hanime_mount::begin(budgetMs);
  bool ok=tfSpi.begin(16,18,17,15);
  if(ok)ok=SD.begin(15,tfSpi,TfRuntimeHz,"/sd",ime_device::Resources::MaximumOpenFiles,false);
  else hanime_mount::fail("SPI_begin");
  if(hanime_mount::expired()&&ok){SD.end();ok=false;}
  const auto diagnostic=hanime_mount::finish();
  return {uint32_t(millis()-start),ok,diagnostic.timed_out,diagnostic.failure?diagnostic.failure:ok?"mounted":diagnostic.stage,diagnostic.mount_result};
 }
 void record(unsigned attempt,const s3tf::MountAttempt& r){
  logLine("[TF_MOUNT_ATTEMPT] attempt=%u elapsed_ms=%u ok=%u timed_out=%u reason=%s fat_result=%d init_hz=400000 runtime_hz=%u\n",attempt,unsigned(r.elapsed_ms),r.mounted,r.timed_out,r.reason,r.fat_result,unsigned(TfRuntimeHz));
 }
};
using Color=ST7306OsptekLpm8::Color;
constexpr Color black=Color::Black,white=Color::White;
void glyph(uint32_t cp,int x,int y,Color color,unsigned scale=1){
 const auto g=scale==2?smallFont.glyph(cp):font.glyph(cp);
 for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
  if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))lcd.pixel(x+g.bearing+col,y+row,color);
}
void label(const char* text,int x,int y,Color color=black,int maxX=294){
 const std::string_view value(text);
 for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
  const auto cp=s3app::display::scalar(value,at);const int width=s3app::display::advance(smallFont,cp);
  if(x+width>maxX)break;glyph(cp,x,y,color,2);x+=width;
 }
}
void menuFill(int x,int y,int w,int h,bool ink){lcd.fillRect(x,y,w,h,ink?black:white);}
void menuText(const s3host::FontPort& face,std::string_view text,int x,int y,bool inverse){
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
  const auto g=face.glyph(s3app::display::scalar(text,at));if(x+g.advance>294)break;
  for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col){
   const int px=x+g.bearing+int(col),py=y+int(row);
   if(px>=0&&px<300&&py>=0&&py<400&&(g.bits[row*g.stride+col/8]&(0x80>>(col%8))))
    lcd.pixel(px,py,inverse?white:black);
  }
  x+=g.advance;
 }
}
void menuAux(std::string_view text,int x,int y,bool inverse){menuText(menuFont14,text,x,y,inverse);}
void menuContent(std::string_view text,int x,int y,bool inverse){menuText(menuFont16,text,x,y,inverse);}
// Keep bounded name-layout scratch off the body/IME draw stack branch.
__attribute__((noinline)) bool drawDocumentMenu(int renameBottom){
 return s3app::display::documentMenu(*app,menuFont16,menuFill,menuAux,menuContent,renameBottom);
}
void memory(const char* stage){
 if(!diagnostics)return;
 if(diagnostics->memory_stage)++diagnostics->superseded;
 diagnostics->memory_stage=stage; // All call sites use static stage literals.
 diagnostics->memory_requested_us=ime_device::clockUs();
 diagnostics->memory_row=0;
}
void memoryStep(){
 if(!diagnostics||!diagnostics->memory_stage)return;
 const char* stage=diagnostics->memory_stage;
 switch(diagnostics->memory_row++){
 case 0:{
 logLine("[MEM] stage=%s requested_us=%llu sample_us=%llu internal_free=%u internal_min=%u internal_largest=%u psram_free=%u psram_min=%u psram_largest=%u loop_stack_free=%u ble_worker_stack_free=%u\n",stage,(unsigned long long)diagnostics->memory_requested_us,(unsigned long long)ime_device::clockUs(),
 heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
 heap_caps_get_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),unsigned(uxTaskGetStackHighWaterMark(nullptr)),unsigned(keyboard::workerStackMinimumBytes()));
  break;
 }
 case 1:{
 logLine("[UI_DIAG] ring_owner_bytes=%u payload_bytes=%u storage=PSRAM capacity=32 dropped=%u short_writes=%u max_flush_us=%u transport=UART0_FIFO\n",unsigned(sizeof(ime_device::uiTrace)),unsigned(sizeof(ime_device::UiTraceRecord)*ime_device::UiTrace::Capacity),unsigned(ime_device::uiTrace.lost()),unsigned(uiShortWrites),unsigned(maxUiFlush));
  break;
 }
 case 2:{
 logLine("[FONT_MEM] ready=%u failed=%u bytes=%u allocation=%s\n",font.ready(),font.failed(),unsigned(ime_device::ChineseFont::Bytes),font.allocation()?(esp_ptr_external_ram(font.allocation())?"PSRAM":"INTERNAL"):"NONE");
  break;
 }
 case 3:{
 if(ime&&ime->window())logLine("[IME_MEM] controller=%s controller_bytes=%u window=%s window_bytes=%u resources=%s resource_owner_bytes=%u\n",esp_ptr_external_ram(ime)?"PSRAM":"INTERNAL",unsigned(sizeof(ime_device::DeviceIme)),esp_ptr_external_ram(ime->window())?"PSRAM":"INTERNAL",unsigned(sizeof(ime_device::CandidateWindow)),esp_ptr_external_ram(imeResources)?"PSRAM":"INTERNAL",unsigned(sizeof(ime_device::Resources)));
  break;
 }
 case 4:{
 if(ime&&ime->navigator()){const auto n=ime->navigator()->memory_usage();const auto c=ime->navigator()->cost();logLine("[IME_NAV] owner=%u impl=%u workspace=%u spans=%u boundaries=%u const_table=%u table_checks=%u char_checks=%u\n",unsigned(n.owner),unsigned(n.impl),unsigned(n.workspace),unsigned(n.spans),unsigned(n.boundaries),unsigned(n.constant_table),unsigned(c.table_entries),unsigned(c.character_comparisons));}
  break;
 }
 case 5:{
 if(ime&&ime->composition()){const auto m=ime->composition()->memory_usage();const auto t=ime->submission()->memory_usage();logLine("[IME_POOLS] composition_impl=%u buffers=%u rank_arrays=%u page_array=%u output_buffers=%u submission_impl=%u capture=%u core_impl=%u core_members=%u core_total_text=%u\n",unsigned(m.impl_bytes),unsigned(m.composition_buffers),unsigned(m.rank_arrays),unsigned(m.page_array),unsigned(m.output_buffers),unsigned(t.impl_bytes),unsigned(t.capture_array_bytes),unsigned(t.core.impl_bytes),unsigned(t.core.members_bytes),unsigned(t.core.total_text_bytes));}
  break;
 }
 case 6:{
 if(app)logLine("[CATALOG] entries=%u configured=%u cache_bytes=%u\n",unsigned(app->catalog.cachedEntries()),unsigned(s3app::ds::DocumentCatalog::CacheEntries),unsigned(app->catalog.cacheBytes()));
  break;
 }
 case 7:{
 if(app)logLine("[ALLOC] app=%s body=%s body_bytes=%u body_capacity=%u snapshot=%s snapshot_bytes=65536 clipboard=%s history=%s staging=%s\n",
 esp_ptr_external_ram(app)?"PSRAM":"INTERNAL",esp_ptr_external_ram(app->sharedState().document.text().data())?"PSRAM":"SSO/INTERNAL",unsigned(app->sharedState().document.text().size()),unsigned(app->sharedState().document.text().capacity()),app->persistence.snapshotAllocation()?(esp_ptr_external_ram(app->persistence.snapshotAllocation())?"PSRAM":"INTERNAL"):"NONE",esp_ptr_external_ram(app->sharedState().editor.clipboardAllocation())?"PSRAM":"INTERNAL",esp_ptr_external_ram(app->sharedState().editor.historyAllocation())?"PSRAM":"INTERNAL",esp_ptr_external_ram(app->sharedState().editor.stagingAllocation())?"PSRAM":"INTERNAL");  break;
 }
 case 8:{
 if(ime&&ime->composition()){
  const auto* c=ime->composition();const auto m=c->planning_memory_usage();const auto r=c->planning_report();
  logLine("[IME_PLAN] current=%u input=%llu revision=%llu generation=%llu limited=%u end=%u queries=%u hits=%u direct=%u local_work=%u core_work=%u read_bytes=%u owner=%u impl=%u buffers=%u const_table=%u\n",unsigned(c->plan_is_current(r)),(unsigned long long)r.input_id,(unsigned long long)r.revision,(unsigned long long)r.generation,unsigned(r.limited),unsigned(r.end),unsigned(r.queries),unsigned(r.cache_hits),unsigned(r.direct_range),unsigned(r.local_work),unsigned(r.core_cost.work_items),unsigned(r.core_cost.read_bytes),unsigned(m.owner),unsigned(m.impl),unsigned(m.buffers),unsigned(m.constant_table));
 }
  break;
 }
 case 9:{
 if(imeResources){const auto index=imeResources->scheme.base_order();const auto& a=ime_device::orderAllocationSnapshot();
  if(index){const auto c=index->locate_stats();logLine("[ORDER_POOL] generation=%u instances=1 capacity=%u resident=%u entry_bytes=%u pool_bytes=%u index_bytes=%u owner_region=%s allocation_count=%u overflow=%u hits=%llu misses=%llu\n",unsigned(a.generation),unsigned(c.capacity),unsigned(c.resident),unsigned(c.entry_bytes),unsigned(c.pool_bytes),unsigned(index->memory_bytes()),esp_ptr_external_ram(index.get())?"PSRAM":"INTERNAL",unsigned(a.count),unsigned(a.overflow),(unsigned long long)c.hits,(unsigned long long)c.misses);}
  else logLine("[ORDER_POOL] instances=0 generation=%u\n",unsigned(a.generation));
 }break;
 }
 case 10:case 11:case 12:case 13:case 14:case 15:case 16:case 17:{
 const auto& a=ime_device::orderAllocationSnapshot();const auto i=diagnostics->memory_row-11;
 if(i<a.count){const auto& r=a.records[i];logLine("[ORDER_ALLOC] generation=%u slot=%u bytes=%u kind=%u region=%s scope=index_construction_task address=%x\n",unsigned(a.generation),unsigned(i),unsigned(r.bytes),unsigned(r.kind),r.region==2?"PSRAM":r.region==1?"INTERNAL":"UNKNOWN",unsigned(r.address));}
 break;
 }
 case 18:{
 const auto p=ime_device::CandidateBuildDiagnostics::pool(ime?ime->window():nullptr);
 logLine("[N1_POOL] arm=%s capacity=%u bytes=%u attempted=%u live=%u region=%s policy=PSRAM_ONLY_NO_INTERNAL_FALLBACK lifetime=query\n",ime_device::CandidateArm,unsigned(HANIME_N1_ROWS),unsigned(ime_device::CandidateBuildDiagnostics::poolBytes()),unsigned(p.attempted),unsigned(p.address!=nullptr),p.address?(esp_ptr_external_ram(p.address)?"PSRAM":"INTERNAL"):"NONE");
 break;
 }
 case 19:case 20:case 21:case 22:{
 if(ime){const auto slot=diagnostics->memory_row-20;const auto& storage=ime->memberStorage();const auto& r=storage.records()[slot];
 logLine("[MEM1_ARRAY] role=%s backend=MEM1 policy=PSRAM_ONLY bytes=%u alignment=%u allocated=%u released=%u failed=%u live=%u region=%s violations=%u allocated_bytes=%llu released_bytes=%llu\n",ime_device::memberRoleName(slot),unsigned(r.bytes),unsigned(r.alignment),unsigned(r.allocated),unsigned(r.released),unsigned(r.failed),unsigned(r.live),ime_device::memberRegionName(r.region),unsigned(storage.violations()),(unsigned long long)r.allocated_bytes,(unsigned long long)r.released_bytes);}
 break;
 }
 default:diagnostics->memory_stage=nullptr;break;
 }
}

// Keep the H scratch arrays off the V call chain. The target compiler measured
// 3728 B draw + 4544 B V paint before this split, above the 8192 B loop stack.
__attribute__((noinline)) void drawHorizontalBody(){
 s3app::paintHorizontalBody(*app,font,
  [](int x,int y,int w,int h,bool ink){lcd.fillRect(x,y,w,h,ink?black:white);},
  [](uint32_t cp,int x,int y,bool ink){glyph(cp,x,y,ink?black:white);});
}

void draw(){
#if DRAFTOR_USB_ENABLED
 if(wifiRuntime&&wifiRuntime->transfer.panel()){
  s3app::paintWifiPanel(wifiRuntime->transfer,menuFont16,menuFill,menuAux,menuContent);
  if(!displaySchedule.presentCompared(lcd.framebuffer(),lcd.BufferSize,millis(),[](const uint8_t* previous,size_t size){return lcd.update(previous,size);}))return;
  inputRoute.framePresented(app->focusToken());keyboard::publishFrame(inputRoute.epoch(),0);
  lastDraw=millis();displaySchedule.completed(lastDraw);app->dirtyDisplay=false;return;
 }
 if(usbActive()){
  s3app::paintUsbPanel(usbRuntime->transfer,menuFont16,menuFill,menuAux,menuContent);
  if(!displaySchedule.presentCompared(lcd.framebuffer(),lcd.BufferSize,millis(),[](const uint8_t* previous,size_t size){return lcd.update(previous,size);}))return;
  // A modal frame advances only the input route, never publishes body/IME pixels.
  inputRoute.framePresented(app->focusToken());keyboard::publishFrame(inputRoute.epoch(),0);
  lastDraw=millis();displaySchedule.completed(lastDraw);app->dirtyDisplay=false;return;
 }
#endif

 if(ime)keyboard::publishImeContext(ime->ingressContext(),ime->resourceOwner());
 s3profile::Scope timing(s3profile::Draw);
 const auto draw_started_us=ime_device::clockUs();
 CPATTR_TIMER(cp_draw,Draw);CPATTR(cpattr::drawStart());
 uint32_t started=micros();lcd.fill(white);
 const auto presentation=app?app->presentationFrame():s3app::PresentationFrame{};
 const bool vertical=app&&!app->horizontalImeSurface();
 if(app&&!ime_device::prepareImePresentation(ime,*app,true))return;
 s3app::VerticalPaintResult verticalFrame;
 s3app::find_ui::Receipt findFrame;
 const bool imeVisible=ime_device::candidateSurfaceVisible(ime,app);
 const bool menuPage=app&&!fatal&&(s3app::display::menuSurface(*app)||app->progressDrawing()||app->commandBarrier());
 const int compositionY=ime&&ime->expanded()?210:278;
 if(app&&!vertical)app->layout.setHeightPx(s3app::editor_geometry::bodyHeight(app->findOpen(),imeVisible,compositionY));
 s3app::MetadataFont metadataFont(font);
 const bool editorPage=app&&!fatal&&!menuPage&&!app->statsOpen();
 auto ble=keyboard::status();
 if(fatal){label(fatal,8,50);label("Save 115200 log",8,80);if(tfMountFailed){label("Temporary recovery:",8,110);label("Power OFF board AND TF, then ON",8,140);}else label("See log before retry",8,110);}
 else if(app){
  if(app->progressDrawing()||app->commandBarrier()){
   s3app::display::secondary::header("正在处理",menuFill,menuAux);
   s3app::display::secondary::wrapped(s3app::display::menuProgress(app->notice),menuFont14,8,70,284,24,330,menuAux);
  }else if(drawDocumentMenu(imeVisible?compositionY-12:328)){
  }else if(app->statsOpen()){
   s3app::display::statistics(app->statsReport(),menuFont16,menuFill,menuAux,menuContent);
  }else{
   s3app::editor_hud::title(app->currentName,vertical,metadataFont,[](int x,int y){lcd.pixel(x,y,black);});
   if(vertical){
    const s3app::VerticalPaintPort port{
     [](void*,draftor::vertical::Rect r,s3app::VerticalColor c){lcd.fillRect(r.x,r.y,r.width,r.height,c==s3app::VerticalColor::White?white:c==s3app::VerticalColor::Red?Color::Red:black);},
     [](void*,uint16_t x,uint16_t y,s3app::VerticalColor c){lcd.pixel(x,y,c==s3app::VerticalColor::White?white:c==s3app::VerticalColor::Red?Color::Red:black);},nullptr,nullptr};
    verticalFrame=s3app::paintVerticalView(*app,app->verticalPresentation(),app->findOpen()?nullptr:ime,font,millis(),port);
    if(verticalFrame.status!=draftor::vertical::Status::Ready){app->candidateVisual.complete(false);return;}
   }else{
   drawHorizontalBody();
   }
   if(app->findOpen())findFrame=s3app::find_ui::paint(*app,ime,{
    [](void*,int x,int y,bool ink){lcd.pixel(x,y,ink?black:white);},nullptr});
  }
 }
 if(menuPage&&!app->renameOpen()&&!app->commandBarrier()&&!app->progressDrawing())
  s3app::display::menuFeedback(*app,menuFill,menuAux);
 if(imeVisible&&!vertical&&!app->findOpen()){
  lcd.fillRect(0,compositionY-4,300,1,black);
  auto textAt=[&](std::string_view text,int x,int y,Color color,int limit){
   for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
    const auto cp=s3app::display::scalar(text,at);const auto g=font.glyph(cp);
    if(x+g.advance>limit){glyph(0x2026,std::max(8,limit-28),y,color);break;}
    glyph(cp,x,y,color);x+=g.advance;
   }
  };
  if(auto* composition=ime->composition()){
   const auto snap=composition->snapshot();const auto selectedView=ime->selectedViewport(font);int selectedX=8;
   for(size_t m=0;m<snap.member_count;++m){const auto& member=snap.members[m];
    const auto first=std::max(selectedView.begin,member.text_begin);
    const auto last=std::min(selectedView.end,member.text_begin+member.text_size);
    if(first>=last)continue;
    const auto part=snap.selected_text.substr(first,last-first);
    textAt(part,selectedX,compositionY+(ime->raw().empty()?0:s3app::ImeCompositionFontMetrics::horizontal_selected_y),member.valid?black:Color::Red,294);
    for(size_t at=0;at<part.size();at=editor::nextUtf8Boundary(part,at))selectedX+=font.glyph(s3app::display::scalar(part,at)).advance;
   }
   if(selectedView.left)lcd.fillRect(3,compositionY+12,2,5,Color::Red);
   if(selectedView.right)lcd.fillRect(296,compositionY+12,2,5,Color::Red);
  }
  s3app::paintHorizontalRaw(*ime,compositionY,{
   [](void*,int x,int y){lcd.pixel(x,y,black);},
   [](void*,int x,int y,unsigned w,unsigned h){lcd.fillRect(x,y,w,h,Color::Red);},nullptr});
  int x=8;
  const auto* view=ime->view();const bool candidatesAvailable=ime->candidateAvailable();
  if(view){for(size_t r=0;r<view->count;++r){const auto& row=view->rows[r];x=8;const int y=compositionY+48+int(r)*34;
   for(size_t c=0;c<row.count;++c){const auto& item=row.items[c];const bool selected=candidatesAvailable&&row.index==view->row&&c==view->col;
    app->candidateVisual.captureHorizontal({item.text.data(),item.bytes},x,y,item.width);
    if(candidatesAvailable&&row.index==view->row){char labelText[2]={char('1'+c),0};label(labelText,x,y+8,selected?Color::Red:black);}
    textAt({item.text.data(),item.bytes},x+14,y,selected?Color::Red:black,std::min(294,x+int(item.width)-4));x+=item.width;
   }
  }}
  const auto& pending=app->candidateVisual;
  if(pending.pending()){
   for(size_t i=0;i<pending.text.count;++i){const auto& item=pending.text.items[i];
    textAt({item.text.data(),item.bytes},item.box.x+14,item.box.y,black,item.limit);
   }
  }
  if(ime->boundaryVisible(millis())){auto rect=ime_device::boundaryRect(ime->boundaryEdge(),compositionY,view?view->count:0);
   if(ime->boundaryEdge()==ime_device::BoundaryEdge::RawLeft||ime->boundaryEdge()==ime_device::BoundaryEdge::RawRight){rect.y=compositionY+s3app::ImeCompositionFontMetrics::horizontal_ink_y;rect.height=s3app::ImeCompositionFontMetrics::horizontal_ink_height;}
   lcd.fillRect(rect.x,rect.y,rect.width,rect.height,Color::Red);}
 }
 if(app&&app->findOpen()){
  if(ble.phase==keyboard::Phase::Pairing&&ble.passkey<1000000){
   char pin[64];std::snprintf(pin,sizeof(pin),"Type %06lu then Enter",(unsigned long)ble.passkey);
   lcd.fillRect(0,380,300,20,white);label(pin,8,383);findFrame.candidates=false;
  }
 }else if(editorPage){
  const auto status=s3app::editor_hud::state(*app,ime,
   imeResources?imeResources->rules():hanime_extension::SchemeRules::R1Xiaohe,ble,loadingStatus.shown());
  s3app::editor_hud::footer(status,metadataFont,
   [](int x,int y,int w,int h){lcd.fillRect(x,y,w,h,white);},
   [](int x,int y){lcd.pixel(x,y,black);});
 }else if(vertical&&!menuPage){
  char status[96];
  const bool loading=loadingStatus.shown().stage!=ime_device::LoadStage::Ready&&loadingStatus.shown().stage!=ime_device::LoadStage::Idle;
  if(ble.phase==keyboard::Phase::Pairing&&ble.passkey<1000000)std::snprintf(status,sizeof(status),"Pair: type %06lu + Enter",(unsigned long)ble.passkey);
  else if(loading)std::snprintf(status,sizeof(status),"%s",loadingStatus.shown().text);
  else if(imeVisible&&ime->receipt().status==hanime::CompositionStatus::UnknownOutcome)std::snprintf(status,sizeof(status),"Unknown: retained, no replay");
  else if(imeVisible&&ime->receipt().status==hanime::CompositionStatus::RetryableNoEffect)std::snprintf(status,sizeof(status),"Rejected: Space retry / Esc edit");
  else if(imeVisible&&ime->active())std::snprintf(status,sizeof(status),"%s",ime_device::verticalStatus(*ime));
  else if(ble.phase!=keyboard::Phase::Ready)std::snprintf(status,sizeof(status),"%s",keyboard::phaseName(ble.phase));
  else std::snprintf(status,sizeof(status),"%u B | pos %u | %s",unsigned(app->document.text().size()),unsigned(app->document.cursorByte()),app->notice);
  s3app::VerticalHudLine footer;s3app::hudAppend(footer,status,smallFont);label(footer.text,292-int(footer.width),383);
 }else if(ble.phase==keyboard::Phase::Pairing&&ble.passkey<1000000){char pin[70];snprintf(pin,sizeof(pin),"Type %06lu then Enter on keyboard",(unsigned long)ble.passkey);lcd.fillRect(0,340,300,60,white);label(pin,8,350);}
 else if(ble.phase==keyboard::Phase::Ready&&imeVisible&&loadingStatus.shown().stage!=ime_device::LoadStage::Ready){
  char rawStatus[64];std::snprintf(rawStatus,sizeof(rawStatus),"Raw buffered: %u | %s",unsigned(ime->raw().size()),ime->raw().empty()?ime->notice():"not in body");label(rawStatus,8,383);
 }else if(ble.phase==keyboard::Phase::Ready&&imeVisible){char caption[64];ime->editCaption(caption,sizeof(caption));label(caption,8,383);}
 else if(!menuPage)label(keyboard::phaseName(ble.phase),8,383);
 const auto transfers=displaySchedule.sent;
 const auto transfer_started_us=ime_device::clockUs();
 if(app){BT_OBSERVE(displayBegin(*app,ime,imeVisible&&ime->candidateAvailable()));}
 if(!displaySchedule.presentCompared(lcd.framebuffer(),lcd.BufferSize,millis(),[](const uint8_t* previous,size_t size){return lcd.update(previous,size);})){BT_OBSERVE(displayEnd(false));if(app)app->candidateVisual.complete(false);return;}
 BT_OBSERVE(displayEnd(true));BT_MEASURE(drawn(vertical,draw_started_us));
 const auto lcd_completed_us=ime_device::clockUs();
 CPATTR(cp_draw.stop(lcd_completed_us);cpattr::presented(transfer_started_us,lcd_completed_us,displaySchedule.sent!=transfers));
 const auto previousImeFrame=ime&&ime->window()?ime->window()->onScreenGeneration():0;
 const auto publication=app?ime_device::publishActivePresentation(*app,ime,presentation,verticalFrame.ime,findFrame):ime_device::PresentationPublication{};
 if(app&&!publication.accepted){app->candidateVisual.complete(false);return;}
 if(app)app->candidateVisual.complete(true);
 if(app)app->fileFramePresented(lcd_completed_us,app->progressDrawing()||app->commandBarrier());
 // Publish one matching route/frame pair immediately after the LCD update,
 // before any diagnostic output can block. Notifications during drawing retain
 // the previous pair. The physical transfer's final-instruction gap is not an
 // atomic hardware boundary; never hold a BLE spinlock across the LCD transfer.
 if(app&&!app->commandBarrier())inputRoute.framePresented(app->focusToken());
 keyboard::publishFrame(inputRoute.epoch(),publication.candidates);
#if DRAFTOR_AUTH_OBSERVE
 if(imeVisible&&ime)ime_device::traceServiceFeedback(ime->composition(),ime->window(),lcd_completed_us,uint8_t(displaySchedule.sent!=transfers));
#endif
 if(imeVisible&&ime->window()&&previousImeFrame!=ime->window()->onScreenGeneration()){
  const auto& v=ime->window()->view();const ime_device::VisibleItem* item=nullptr;
  if(v.count&&v.row>=v.rows[0].index&&v.row<v.rows[0].index+v.count){const auto& row=v.rows[v.row-v.rows[0].index];if(v.col<row.count)item=&row.items[v.col];}
  ime_device::traceUi(ime_device::UiTraceKind::Lcd,ime->composition(),ime->window(),nullptr,item,displaySchedule.sent==transfers?1:0,v.action,lcd_completed_us);
  CPATTR(cpattr::finish(lcd_completed_us,v.generation,v.action,ime->window()->querySerial());if(diagnostics)cpattr::emit(diagnostics->output));
  logLine("[PRESENT_TIMING] action=%u frame=%u draw_start_us=%llu transfer_start_us=%llu done_us=%llu transmitted=%u\n",unsigned(v.action),unsigned(v.generation),(unsigned long long)draw_started_us,(unsigned long long)transfer_started_us,(unsigned long long)lcd_completed_us,displaySchedule.sent!=transfers);
 }
 if(ime&&ime->acceptedAction()!=lastBodyFrameAction&&ime->acceptedDocumentCurrent()){
  lastBodyFrameAction=ime->acceptedAction();
  ime_device::traceUi(ime_device::UiTraceKind::BodyLcd,ime->composition(),ime->window(),nullptr,nullptr,0,lastBodyFrameAction,lcd_completed_us);
 }
#if DRAFTOR_DISPLAY_TX_OBSERVE
 if(diagnostics){
  const auto* window=ime?ime->window():nullptr;
  display_tx_observe::emit(diagnostics->output,displaySchedule.present_serial,
   displaySchedule.sent!=transfers,lcd.lastTransfer(),transfer_started_us,lcd_completed_us,
   window?window->view().action:0,window?window->querySerial():0,publication.candidates,
   ime?ime->acceptedAction():0,presentation.revision);
 }
#endif
 if(imeVisible&&imeTiming.frame_pending&&imeResources&&imeResources->ready()&&ime&&ime->window()&&!ime->window()->busy()&&ime->window()->state()==ime_device::WindowState::Ready&&!ime->raw().empty()){
  const auto now=lcd_completed_us;logLine("[IME_FIRST_FRAME] request=%u uptime_us=%llu from_raw_us=%llu from_request_us=%llu rows=%u raw=%u from_last_key_us=%llu from_boot_log_us=%llu diagnostic=%u\n",unsigned(imeTiming.serial),(unsigned long long)now,(unsigned long long)(now-imeTiming.first_raw_us),(unsigned long long)(imeTiming.request_us?now-imeTiming.request_us:0),unsigned(ime->view()->count),unsigned(imeTiming.raw_serial),(unsigned long long)(now-imeTiming.last_raw_us),(unsigned long long)(now-bootLogUs),probeUsedThisBoot);imeTiming.frame_pending=false;
 }
 if(response.awaiting&&app&&!app->commandBarrier()){if(app->focusToken()==response.pending_focus&&!app->filesOpen()&&!app->statsOpen())response.present_ms=std::max(response.present_ms,millis()-response.pending_ms);response.awaiting=false;}
 maxDraw=std::max(maxDraw,micros()-started);lastDraw=millis();displaySchedule.completed(lastDraw);if(app)app->dirtyDisplay=false;
}
// The route epoch is published after the driver returns (not physical panel settling). Notifications received
// during open/read/verify/reflow/draw retain the previous context.
void logFileProgress(){
 if(!app)return;
 static uint32_t serial=0;static uint8_t flags=0;
 const auto& t=app->fileTiming;if(!t.serial)return;
 const uint8_t next=(t.feedback_us?1:0)|(t.start_us?2:0)|(t.complete_us?4:0)|(t.body_lcd_us?8:0)|(t.cancelled?16:0);
 if(serial==t.serial&&flags==next)return;
 serial=t.serial;flags=next;
 logLine("[FILE_FLOW] request=%u command=%u key_ms=%u request_us=%llu feedback_lcd_us=%llu start_us=%llu install_us=%llu complete_us=%llu body_lcd_us=%llu cancelled=%u\n",
  unsigned(t.serial),unsigned(t.command),unsigned(t.key_ms),(unsigned long long)t.request_us,(unsigned long long)t.feedback_us,(unsigned long long)t.start_us,(unsigned long long)t.install_us,(unsigned long long)t.complete_us,(unsigned long long)t.body_lcd_us,unsigned(t.cancelled));
}
void presentContext(){
 logFileProgress();
 if(!app||app->commandBarrier()||!inputRoute.needsFrame(app->focusToken()))return;
 repeat.clear();draw();logFileProgress();
 if(s3profile::active&&!app->commandBarrier()){
  const auto result=s3profile::finish();
  if(diagnostics){
   if(diagnostics->profile_row<2)++diagnostics->superseded;
   diagnostics->profile=result;diagnostics->profile_row=0;
   diagnostics->profile_us=ime_device::clockUs();
   diagnostics->profile_status=hanime::documents::statusMessage(app->lastOperation.status);
   diagnostics->profile_bytes=app->sharedState().document.text().size();diagnostics->profile_cursor=app->sharedState().document.cursorByte();
  }
 }

}
void flushUiTrace(){
 if(!diagnostics)return;
 auto& d=*diagnostics;
 if(ime_device::drainUiTracePart(ime_device::uiTrace,d.ui,d.ui_part,d.output))return;
 if(d.output.full())return;
 const auto& trace=ime_device::uiTrace;
 if(d.ui_text_lost!=trace.textLost()||d.ui_lost!=trace.lost()||d.ui_keyups!=trace.aggregatedKeyups()||d.ui_duplicates!=trace.aggregatedDuplicates()){
  if(d.output.printf("[UI_LOST] count=%u aggregated_keyups=%u aggregated_duplicates=%u text_lost=%u peak=%u capacity=32 scope=trace_records_not_user_actions\n",unsigned(trace.lost()),unsigned(trace.aggregatedKeyups()),unsigned(trace.aggregatedDuplicates()),unsigned(trace.textLost()),unsigned(trace.peak()))){
   d.ui_text_lost=trace.textLost();d.ui_lost=trace.lost();d.ui_keyups=trace.aggregatedKeyups();d.ui_duplicates=trace.aggregatedDuplicates();
  }
 }

}

void serialDiagnostic(){
#if DRAFTOR_TELEMETRY_CAPTURE
 return;
#endif
 if(tfProbe)tfProbe->expireAt(ime_device::clockUs());
 if(tfProbe&&ime&&ime->chinese())tfProbe->cancel();
 // Diagnostic commands never enter HID routing or the final document model.
 for(unsigned i=0;i<32&&Serial.available();++i){
  const auto ch=char(Serial.read());
  if(ch=='\r')continue;
  if(ch!='\n'){
   if(serialCommandSize+1<sizeof(serialCommand))serialCommand[serialCommandSize++]=ch;
   else serialOverflow=true;
   continue;
  }
  serialCommand[serialCommandSize]=0;
  if(!serialOverflow&&std::strcmp(serialCommand,"TFCANCEL")==0){if(tfProbe)tfProbe->cancel();}
  else if(!serialOverflow&&(std::strcmp(serialCommand,"DIAG0")==0||std::strcmp(serialCommand,"DIAG1")==0)){
   if(logGate().idle()&&!tfProbe){detailedDiagnostics=serialCommand[4]=='1';ime_device::uiTrace.setDetailed(detailedDiagnostics);logLine("[DIAG_MODE] detailed=%u counters=always_on prefetch=%u\n",detailedDiagnostics,rowPrefetchEnabled);}
   else logLine("[DIAG_MODE] rejected; wait for foreground/save completion\n");
  }
  else if(!serialOverflow&&(std::strcmp(serialCommand,"TRACE0")==0||std::strcmp(serialCommand,"TRACE1")==0||std::strcmp(serialCommand,"TRACE2")==0)){
   const auto mode=uint8_t(serialCommand[5]-'0');ime_device::uiTrace.setMode(mode);detailedDiagnostics=mode==2;
   logLine("[TRACE_SCHEMA] version=2 mode=%u raw_text=%u reset_on_BLE_epoch=1 scope=driver_return_not_physical_scan\n",unsigned(mode),unsigned(mode==2));
  }
  else if(!serialOverflow&&(std::strcmp(serialCommand,"PREFETCH0")==0||std::strcmp(serialCommand,"PREFETCH1")==0)){
   if(ime&&!ime->active()&&!tfProbe){rowPrefetchEnabled=serialCommand[8]=='1';logLine("[IME_PREFETCH] enabled=%u diagnostic=1 scope=bounded_next_pixel_row\n",rowPrefetchEnabled);}
   else logLine("[IME_PREFETCH] rejected; finish/cancel input first\n");
  }
  else if(!serialOverflow&&std::strcmp(serialCommand,"TFPROBE")==0){
   if(!app||!imeResources||!ime||ime->chinese()||ime->active()||app->commandBarrier()||tfProbe||ime_device::metrics.resource_handles>1){
    logLine("[TF_PROBE_END] ok=0 reason=need_idle_EN_and_resource_handles_le_1\n");
   }else{
    try{
     probeUsedThisBoot=true;tfProbe=std::make_unique<ime_device::TfProbe>(std::string(ime_device::CandidateResourceRoot)+"/shared.hnv",imeResources->mount());
     logLine("[TF_PROBE_BEGIN] firmware=%s task=STARTUP_REPAIR_V1_0 built=%s %s uptime_us=%llu frequency=%u font_read_block=%u packet_sha=aab7e1c1eea4a7c5224f4babd25f1cf55206b75f40eb0599acddbac89628bf60 samples_only=1 automatic_ready=0 memory=%u storage=%s limit_us=60000000\n",Firmware,__DATE__,__TIME__,(unsigned long long)ime_device::clockUs(),unsigned(TfRuntimeHz),unsigned(ime_device::ChineseFont::ReadBytes),unsigned(sizeof(ime_device::TfProbe)),esp_ptr_external_ram(tfProbe.get())?"PSRAM":"INTERNAL");
    }catch(const std::bad_alloc&){logLine("[TF_PROBE_END] ok=0 reason=diagnostic_allocation_failed\n");}
   }
  }
  serialCommandSize=0;serialOverflow=false;
 }
}
void finishProbe(){
 if(!tfProbe)return;
 if(tfProbe->hasResult()){
  const auto r=tfProbe->result();
  logLine("[TF_PROBE_CASE] id=%u pattern=%s request_bytes=%u ok=%u crc=%08x expected=%08x first_us=%llu last_us=%llu exec_us=%llu wall_us=%llu\n",r.id,ime_device::TfProbe::pattern(r.id),unsigned(r.request_bytes),r.ok,unsigned(r.crc),unsigned(r.expected),(unsigned long long)r.first_us,(unsigned long long)r.last_us,(unsigned long long)r.exec_us,(unsigned long long)(r.last_us-r.first_us));
  logLine("[TF_PROBE_IO] id=%u requests=%llu requested_bytes=%llu reads=%llu bytes=%llu hits=%llu seeks=%llu read_us=%llu seek_us=%llu max_read_us=%llu max_seek_us=%llu\n",r.id,(unsigned long long)r.io.requests,(unsigned long long)r.io.requested_bytes,(unsigned long long)r.io.reads,(unsigned long long)r.io.bytes,(unsigned long long)r.io.hits,(unsigned long long)r.io.seeks,(unsigned long long)r.io.read_us,(unsigned long long)r.io.seek_us,(unsigned long long)r.max_read_us,(unsigned long long)r.max_seek_us);
  tfProbe->acknowledge();
 }
 if(tfProbe->finished()){
  logLine("[TF_PROBE_END] ok=%u reason=%s resource_state=%u\n",!*tfProbe->error(),*tfProbe->error()?tfProbe->error():"sample_bytes_match_not_full_validation",unsigned(imeResources->state()));
  tfProbe.reset();memory("tf-probe-released");
 }
}
void diagnosticStep(){
 constexpr unsigned phases=unsigned(ime_device::Phase::Count),sizes=ime_device::IoProfile::Sizes;
 constexpr unsigned end=phases+sizes+unsigned(ime_device::IoKind::Count)*sizes;
 if(!diagnostics)return;
 if(diagnosticRow>end+3+3*unsigned(ime_device::FileRole::Count)){diagnosticRow=1000;return;}
 if(!detailedDiagnostics&&diagnosticRow>=phases&&diagnosticRow<end)diagnosticRow=end;
 const auto start=micros();const unsigned row=diagnosticRow++;
 if(row<phases){
  const auto& m=diagnostics->metrics.phases[row];
  logLine("[IME_PHASE] phase=%u steps=%llu us=%llu max_us=%llu reads=%llu bytes=%llu work=%llu first_us=%llu last_us=%llu physical_calls=%llu physical_bytes=%llu hits=%llu seeks=%llu read_us=%llu seek_us=%llu\n",row,(unsigned long long)m.steps,(unsigned long long)m.us,(unsigned long long)m.max_us,(unsigned long long)m.read_calls,(unsigned long long)m.read_bytes,(unsigned long long)m.work,(unsigned long long)m.first_us,(unsigned long long)m.last_us,(unsigned long long)m.io.reads,(unsigned long long)m.io.bytes,(unsigned long long)m.io.hits,(unsigned long long)m.io.seeks,(unsigned long long)m.io.read_us,(unsigned long long)m.io.seek_us);
 }else if(row<phases+sizes){
  const auto i=row-phases;
  if(diagnostics->io.logical[i])logLine("[IO_LOGICAL_SIZE] uptime_us=%llu bin=%u max_bytes=%u calls=%llu hits=%llu\n",(unsigned long long)ime_device::clockUs(),i,unsigned(ime_device::IoProfile::Limits[i]),(unsigned long long)diagnostics->io.logical[i],(unsigned long long)diagnostics->io.logical_hits[i]);
 }else if(row<end){
  const auto n=row-phases-sizes,k=n/sizes,b=n%sizes;const auto& v=diagnostics->io.samples[k][b];
  if(v.calls)logLine("[IO_CALL] uptime_us=%llu kind=%s bin=%u calls=%llu bytes=%llu us=%llu max_us=%llu errors=%llu\n",(unsigned long long)ime_device::clockUs(),ime_device::IoProfile::name(k),b,(unsigned long long)v.calls,(unsigned long long)v.bytes,(unsigned long long)v.us,(unsigned long long)v.max_us,(unsigned long long)v.errors);
 }else if(row==end){
  const auto& v=diagnostics->io.resource_during_save;
  logLine("[IO_SAVE_OVERLAP] calls=%llu bytes=%llu us=%llu max_us=%llu errors=%llu diagnostic_row_max_us=%u\n",(unsigned long long)v.calls,(unsigned long long)v.bytes,(unsigned long long)v.us,(unsigned long long)v.max_us,(unsigned long long)v.errors,unsigned(maxDiagnosticUs));
 }
 if(row>end&&row<=end+3&&imeResources){
  const bool shared=row==end+1,scheme=row==end+2;const auto v=diagnostics->auth[row-end-1];const auto m=diagnostics->auth_memory[row-end-1];
  logLine("[IME_AUTH] role=%s status=%u whole_file_checked=%u total_blocks=%llu resident=%u index_resident=%u loads=%llu index_loads=%llu requests=%llu bytes=%llu cache_only=%llu cache_only_bytes=%llu data_hits=%llu index_hits=%llu io_calls=%u io_bytes=%u work=%u payload=%u metadata=%u owner_fixed=%u inner_allocation=UNMEASURED\n",shared?"shared":scheme?"scheme":"order",unsigned(v.status),v.whole_file_checked,(unsigned long long)v.total_blocks,unsigned(v.resident_verified),unsigned(v.resident_verified_index_pages),(unsigned long long)v.authenticated_loads,(unsigned long long)v.authenticated_index_loads,(unsigned long long)v.logical_requests,(unsigned long long)v.logical_bytes,(unsigned long long)v.cache_only_requests,(unsigned long long)v.cache_only_bytes,(unsigned long long)v.data_cache_hits,(unsigned long long)v.index_cache_hits,unsigned(v.cost.read_calls),unsigned(v.cost.read_bytes),unsigned(v.cost.work_items),unsigned(m.data_payload_bytes+m.index_payload_bytes),unsigned(m.data_metadata_bytes+m.index_metadata_bytes),unsigned(m.owner_fixed_bytes));
 }
 if(row>end+3&&row<=end+3+3*unsigned(ime_device::FileRole::Count)){
  const auto n=row-end-4,k=n/3,part=n%3;const auto& r=diagnostics->io.roles[k];using U=unsigned long long;
  if(part==0)logLine("[IO_ROLE] snapshot_us=%llu role=%s logical=%llu hits=%llu reads=%llu bytes=%llu seeks=%llu position_match=%llu errors=%llu\n",U(diagnostics->summary_us),ime_device::IoProfile::roleName(k),U(r.requests),U(r.hits),U(r.reads),U(r.bytes),U(r.seeks),U(r.position_matches),U(r.errors));
  else if(part==1)logLine("[IO_ROLE_COST] snapshot_us=%llu role=%s read_us=%llu seek_us=%llu max_read_us=%llu max_seek_us=%llu backward=%llu forward=%llu max_jump=%llu max_offset=%llu\n",U(diagnostics->summary_us),ime_device::IoProfile::roleName(k),U(r.read_us),U(r.seek_us),U(r.max_read_us),U(r.max_seek_us),U(r.backward),U(r.forward),U(r.max_jump),U(r.max_offset));
  else logLine("[IO_ROLE_BINS] snapshot_us=%llu role=%s sizes_16_64_256_512_1k_4k_more=%u,%u,%u,%u,%u,%u,%u jumps_512_4k_64k_1m_16m_more=%u,%u,%u,%u,%u,%u marginal_only=1\n",U(diagnostics->summary_us),ime_device::IoProfile::roleName(k),unsigned(r.sizes[0]),unsigned(r.sizes[1]),unsigned(r.sizes[2]),unsigned(r.sizes[3]),unsigned(r.sizes[4]),unsigned(r.sizes[5]),unsigned(r.sizes[6]),unsigned(r.jumps[0]),unsigned(r.jumps[1]),unsigned(r.jumps[2]),unsigned(r.jumps[3]),unsigned(r.jumps[4]),unsigned(r.jumps[5]));
 }
 maxDiagnosticUs=std::max(maxDiagnosticUs,micros()-start);
}
void requestSummary(){
 if(!diagnostics)return;
 auto& d=*diagnostics;
 if(d.summary_row<11||diagnosticRow<1000)++d.superseded;
 d.summary_us=ime_device::clockUs();
 d.metrics=ime_device::metrics;d.io=ime_device::ioProfile;
 d.schedule=initSchedule;d.response=response;
 d.display_sent=displaySchedule.sent;d.display_reused=displaySchedule.reused;d.display_failed=displaySchedule.failed;
 if(ime){d.arrow_peak=ime->arrowPeak();d.arrow_overflow=ime->arrowOverflow();d.arrow_discarded=ime->arrowDiscarded();}
 d.load=imeResources?ime_device::loadSnapshot(font,*imeResources):ime_device::LoadSnapshot{};
 d.rows=ime&&ime->window()?ime->window()->costs():ime_device::WindowCosts{};
 const auto ble=keyboard::status();
 d.input_events=ble.received;d.queue_peak=ble.queue_peak;d.queue_overflow=ble.overflow;d.bad_reports=ble.bad_reports;
 d.input_age=maxInputAge;d.draw_us=maxDraw;d.service_us=maxService;d.loop_us=maxLoop;
 if(imeResources){
  d.auth={imeResources->sharedSnapshot(),imeResources->schemeSnapshot(),imeResources->orderSnapshot()};
  d.auth_memory={imeResources->sharedMemory(),imeResources->schemeMemory(),imeResources->orderMemory()};
 }
 d.summary_row=0;diagnosticRow=0;
 const auto pending=response.pending_ms;const auto awaiting=response.awaiting;const auto focus=response.pending_focus;
 response={};response.pending_ms=pending;response.awaiting=awaiting;response.pending_focus=focus;
 memory("running");
}
void summaryStep(){
 auto& d=*diagnostics;
 const auto& im=d.metrics;
 using U=unsigned long long;
 switch(d.summary_row++){
 case 0:
  logLine("[IME_IO] snapshot_us=%llu reads=%llu bytes=%llu max_read_us=%llu max_seek_us=%llu max_open_us=%llu request_max=%u resource_handles=%u document_handles=%u handle_peak=%u stale=%llu reauth=%llu blocked=%llu duplicate=%llu\n",U(d.summary_us),U(im.reads),U(im.bytes),U(im.max_read_us),U(im.max_seek_us),U(im.max_open_us),unsigned(im.largest_read),unsigned(im.resource_handles),unsigned(im.document_handles),unsigned(im.handle_peak),U(im.stale_choices),U(im.reauthorizations),U(im.blocked),U(im.duplicates));
  break;
 case 1:
  logLine("[LOAD_PROGRESS] uptime_us=%llu stage=%u done=%llu total=%llu detail=%u text=%s\n",U(d.summary_us),unsigned(d.load.stage),U(d.load.done),U(d.load.total),d.load.detail,d.load.text);
  break;
 case 2:
  logLine("[IME_CACHE] snapshot_us=%llu logical_calls=%llu logical_bytes=%llu hits=%llu hit_bytes=%llu misses=%llu seeks=%llu seek_us=%llu read_us=%llu live=%u peak=%u alloc_failures=%u scope=platform_cache\n",U(d.summary_us),U(im.requests),U(im.requested_bytes),U(im.hits),U(im.hit_bytes),U(im.misses),U(im.seeks),U(im.seek_us),U(im.read_us),unsigned(im.cache_live_bytes),unsigned(im.cache_peak_bytes),unsigned(im.cache_alloc_failures));
  break;
 case 3:{
  const auto& c=d.rows;
  logLine("[IME_ROWS] snapshot_us=%llu prefetch=%u reused=%llu built=%llu materialized=%llu hits=%llu misses=%llu starts=%llu waits=%llu cancelled=%llu steps=%llu ranges=%llu\n",U(d.summary_us),rowPrefetchEnabled,U(c.reused_rows),U(c.built_rows),U(c.materialized_items),U(c.prefetch_hits),U(c.prefetch_misses),U(c.prefetch_starts),U(c.foreground_waits),U(c.prefetch_cancelled),U(c.prefetch_steps),U(c.range_moves));
  break;
 }
 case 4:{
  const auto& c=d.schedule;
  logLine("[IME_SCHEDULE] snapshot_us=%llu turns=%llu slices=%llu key_blocks=%llu save_turns=%llu save_deferred=%llu max_slice_us=%u max_turn_us=%u init_quantum_us=%u run_quantum_us=%u init_slice_limit=%u run_slice_limit=%u\n",U(d.summary_us),U(c.turns),U(c.slices),U(c.blocked_by_keys),U(c.save_turns),U(c.save_deferred),unsigned(c.max_slice_us),unsigned(c.max_turn_us),unsigned(InitQuantumUs),unsigned(RunQuantumUs),InitSlices,RunSlices);
  break;
 }
 case 5:
  logLine("[RESP] snapshot_us=%llu input_events=%u queue_peak=%u overflow=%u bad_reports=%u max_input_age_ms=%u max_draw_us=%u max_service_us=%u max_loop_us=%u\n",U(d.summary_us),unsigned(d.input_events),unsigned(d.queue_peak),unsigned(d.queue_overflow),unsigned(d.bad_reports),unsigned(d.input_age),unsigned(d.draw_us),unsigned(d.service_us),unsigned(d.loop_us));
  break;
 case 6:{
  const auto& c=d.response;
  logLine("[RESP_WINDOW] snapshot_us=%llu accepted_body=%u accepted_age_ms=%u key_us=%u key_to_frame_ms=%u save_step_us=%u operation_log_us=%u log_scope=RAM_capture_only\n",U(d.summary_us),unsigned(c.accepted),unsigned(c.max_age_ms),unsigned(c.key_us),unsigned(c.present_ms),unsigned(c.save_us),unsigned(c.log_us));
  break;
 }
 case 7:
  if(app){
   const auto& m=app->scans;
   logLine("[READERS] sample_us=%llu find_done=%u cancelled=%u work_max=%u cpu_us=%llu step_max_us=%llu stats_done=%u cancelled=%u bytes_max=%u cpu_us=%llu step_max_us=%llu\n",U(ime_device::clockUs()),unsigned(m.find_completed),unsigned(m.find_cancelled),unsigned(m.max_find_work),U(m.find_us),U(m.max_find_step_us),unsigned(m.stats_completed),unsigned(m.stats_cancelled),unsigned(m.max_stats_bytes),U(m.stats_us),U(m.max_stats_step_us));
  }
  break;
 case 8:{
  const auto& c=d.output.costs();
  logLine("[LOG_QUEUE] detailed=%u queued=%llu dropped=%llu oversized=%llu short=%llu backpressure=%llu deferred=%llu bytes=%llu max_format_us=%llu max_write_us=%llu idle_step_max_us=%u superseded=%u unavailable=%u psram_owner_bytes=%u capacity=%u line_bytes=%u write_turn_bytes=%u transport=uart0_fifo tx_buffer_bytes=0 host_presence=unavailable\n",detailedDiagnostics,U(c.queued),U(c.dropped),U(c.oversized),U(c.short_writes),U(c.backpressure),U(c.deferred),U(c.bytes),U(c.max_format_us),U(c.max_write_us),unsigned(maxDiagnosticUs),unsigned(d.superseded),unsigned(logsUnavailable),unsigned(s3_diagnostic_memory_abi[0]),unsigned(ime_device::IdleLog::Capacity),unsigned(ime_device::IdleLog::LineBytes),unsigned(ime_device::IdleLog::WriteBytesPerTurn));
  break;
 }
 case 9:
  if(ime)logLine("[INPUT_QUEUE] sample_us=%llu capacity=32 queued=%u peak=%u overflow=%u cancelled=%u nav_queued=%u trace_mode=%u text_lost=%u\n",U(ime_device::clockUs()),unsigned(ime->queuedInput()),unsigned(ime->inputPeak()),unsigned(ime->inputOverflow()),unsigned(ime->inputCancelled()),unsigned(ime->queuedArrows()),unsigned(ime_device::uiTrace.mode()),unsigned(ime_device::uiTrace.textLost()));
  logLine("[LOOP_YIELD] tick_blocks=%llu yield_only=%llu max_unblocked_us=%llu scope=cooperative_synchronous_calls_not_preempted\n",U(loopYield.blocking_turns),U(loopYield.yield_only_turns),U(ime_device::LoopYield::MaximumUnblockedUs));
  break;
 case 10:
  logLine("[ADAPTER_PRESENT] snapshot_us=%llu sent=%u unchanged=%u failed=%u mirror_bytes=%u nav_peak=%u nav_overflow=%u nav_discarded=%u nav_capacity=32\n",U(d.summary_us),unsigned(d.display_sent),unsigned(d.display_reused),unsigned(d.display_failed),unsigned(displaySchedule.memoryBytes()),unsigned(d.arrow_peak),unsigned(d.arrow_overflow),unsigned(d.arrow_discarded));
  break;
 }
}
void operationDiagnosticStep(){
 auto& d=*diagnostics;
 if(d.document_pending){
  d.document_pending=false;
  logLine("[DOCUMENT] uptime_us=%llu firmware=%s op=%u status=%s stage=%s body_committed=%u dirty=%u bytes=%u cursor=%u\n",(unsigned long long)d.operation_us,Firmware,unsigned(d.operation),d.operation_status,d.operation_stage.data(),d.body_committed,d.dirty,unsigned(d.body_bytes),unsigned(d.cursor));
 }else if(d.create_row<10){
  static const char* stages[]={"idle","reserve","collection","intent","initial","publish","clear_intent","install","current","finish"};
  const auto row=d.create_row++;
  logLine("[CREATE_STEP] op=%u stage=%s steps=%u sum_us=%llu max_us=%llu\n",unsigned(d.operation),stages[row],unsigned(d.create.steps[row]),(unsigned long long)d.create.us[row],(unsigned long long)d.create.max_us[row]);
 }else if(d.profile_row<2){
  const auto& r=d.profile;
  if(d.profile_row++==0){
   logLine("[OPEN_PROFILE] uptime_us=%llu op=%s status=%s bytes=%u cursor=%u total_us=%llu access_us=%llu read_us=%llu write_us=%llu verify_us=%llu install_us=%llu layout_us=%llu draw_us=%llu main_step_max_us=%llu\n",(unsigned long long)d.profile_us,r.operation,d.profile_status,unsigned(d.profile_bytes),unsigned(d.profile_cursor),(unsigned long long)r.total_us,(unsigned long long)r.us[s3profile::Access],(unsigned long long)r.us[s3profile::Read],(unsigned long long)r.us[s3profile::Write],(unsigned long long)r.us[s3profile::Verify],(unsigned long long)r.us[s3profile::Install],(unsigned long long)r.us[s3profile::Layout],(unsigned long long)r.us[s3profile::Draw],(unsigned long long)r.max_main_step_us);
  }else{
   logLine("[OPEN_IO] op=%s opens=%u entries=%u reads=%u read_bytes=%u max_read=%u writes=%u write_bytes=%u crc_bytes=%u glyph_calls=%u glyph_probes=%u\n",r.operation,unsigned(r.opens),unsigned(r.entries),unsigned(r.reads),unsigned(r.read_bytes),unsigned(r.max_read),unsigned(r.writes),unsigned(r.write_bytes),unsigned(r.crc_bytes),unsigned(r.glyph_calls),unsigned(r.glyph_probes));
  }
 }
}
void idleDiagnostics(){
#if DRAFTOR_AUTH_OBSERVE
 {
 if(!diagnostics||!logGate().transportReady()||diagnostics->output.full())return;
 auto& d=*diagnostics;
 if(d.ui_part<ime_device::UiTraceParts||ime_device::uiTrace.size())ime_device::drainUiTracePart(ime_device::uiTrace,d.ui,d.ui_part,d.output);
 else {
  const auto& costs=d.output.costs();
  uint32_t cp_dropped=0,cp_overwritten=0;
#if DRAFTOR_CP_OBSERVE
  cp_dropped=cpattr::state.dropped;cp_overwritten=cpattr::state.overwritten;
#endif
  if(d.ui_lost!=ime_device::uiTrace.lost()||d.observe_log_dropped!=costs.dropped||d.observe_log_oversized!=costs.oversized||d.observe_cp_dropped!=cp_dropped||d.observe_cp_overwritten!=cp_overwritten){
   if(d.output.printf("[SW_LOST] total=%u peak=%u log_dropped=%llu log_oversized=%llu cp_dropped=%u cp_overwritten=%u\n",unsigned(ime_device::uiTrace.lost()),unsigned(ime_device::uiTrace.peak()),(unsigned long long)costs.dropped,(unsigned long long)costs.oversized,unsigned(cp_dropped),unsigned(cp_overwritten))){
    d.ui_lost=ime_device::uiTrace.lost();d.observe_log_dropped=costs.dropped;d.observe_log_oversized=costs.oversized;d.observe_cp_dropped=cp_dropped;d.observe_cp_overwritten=cp_overwritten;
   }
  }
 }
 return;
 }
#endif
#if DRAFTOR_TELEMETRY_CAPTURE
 return;
#endif
 if(!diagnostics)return;
 const auto gate=logGate();
 const auto start=micros();
 diagnostics->output.service(serialLogPort,gate);
 if(!gate.transportReady()||micros()-start>=500||diagnostics->output.full())return;
 auto& d=*diagnostics;
 // One formatter only. Key/IME/save work is rechecked on every subsequent turn.
 if(gate.idle()&&d.document_pending)operationDiagnosticStep();
 else if(d.ui_part<ime_device::UiTraceParts||ime_device::uiTrace.size()||d.ui_text_lost!=ime_device::uiTrace.textLost()||d.ui_lost!=ime_device::uiTrace.lost()||d.ui_keyups!=ime_device::uiTrace.aggregatedKeyups()||d.ui_duplicates!=ime_device::uiTrace.aggregatedDuplicates())flushUiTrace();
 else if(!gate.idle()){} // During queries only transport and one UI line, never full snapshots.
 else if(d.memory_stage)memoryStep();
 else if(d.create_row<10||d.profile_row<2)operationDiagnosticStep();
 else if(d.summary_row<11)summaryStep();
 else diagnosticStep();
 maxDiagnosticUs=std::max(maxDiagnosticUs,micros()-start);
 maxUiFlush=std::max(maxUiFlush,uint32_t(d.output.costs().max_write_us));
 uiShortWrites=uint32_t(d.output.costs().short_writes);
}
}
void deviceSetup(){
 parkTfPins(); // Park card lines before the display/BLE startup, without power cycling.
 Serial.setTxBufferSize(0);Serial.begin(115200);
 s3app::stability::setup();
#if DRAFTOR_TELEMETRY_CAPTURE
 esp_log_level_set("*",ESP_LOG_NONE);
#if DRAFTOR_RELEASE_BUILD
 Serial.printf("[DRAFTOR_BUILD] firmware=%s release=1 telemetry=0 resources=%s\n",Firmware,ime_device::CandidateResourceRoot);
#else
 Serial.printf("[BT_BUILD] firmware=%s profile=%u capture=1 core=HN_CORE_PERF2_P3_PREFIX_CONTINUATION_C1_20260923 host=HN_INPUT_PERF2_P3_CONSUMER_C1_20260923\n",Firmware,DRAFTOR_TELEMETRY_PROFILE);
#endif
#endif
#if DRAFTOR_AUTH_OBSERVE
 Serial.printf("[SW_CONFIG] window_us=%u init_us=%u slices=%u record=%u capacity=%u capture=%u endpoints=1 sha_final=%u cp=%u shared_variant=%u text_chain=%u block=%u resident=%u structural_host=%u\n",unsigned(RunQuantumUs),unsigned(InitQuantumUs),unsigned(RunSlices),unsigned(sizeof(ime_device::UiTraceRecord)),unsigned(ime_device::UiTrace::Capacity),unsigned(DRAFTOR_TELEMETRY_CAPTURE),unsigned(HANIME_SHA256_BATCHED_FINAL),unsigned(DRAFTOR_CP_OBSERVE),unsigned(DRAFTOR_SHARED_TEXT_VARIANT),unsigned(HANIME_TEXT_CHAIN),unsigned(DRAFTOR_BLOCK_BACKEND),unsigned(DRAFTOR_TEXT_RESIDENT),unsigned(HANIME_STRUCTURAL_HOST_OBSERVE));
#endif
 if(void* memory=heap_caps_malloc(sizeof(DeferredDiagnostics),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))diagnostics=new(memory) DeferredDiagnostics;
 if(diagnostics)ime_device::uiTrace.bind(diagnostics->ui_records.data());
 delay(300);bootLogUs=ime_device::clockUs();logLine("\n[BOOT] firmware=%s task=P23_PHASE2C_R3_R2_RUNTIME_HV core_release=HanIME-Core-0.13.1-rc.1 order_resource_format=%s core_api=0.13.0 extension=0.8.4-rc.1 locate_entries=8 base_order=1 record_slices=1 prefetch=0 auth_shared=2 auth_scheme=1 auth_order=1 release_pin_sha=%s uptime_us=%llu built=%s %s\n",Firmware,ime_device::CandidateOrderFormat,ime_device::CandidatePinSha,(unsigned long long)bootLogUs,__DATE__,__TIME__);
 logLine("[CANDIDATE] firmware=%s arm=%s core_release=HanIME-Core-0.13.1-rc.1 order_resource_format=%s wubi_order_format=2 order_verification_format=1 resource_dir=%s e1=1 n1_rows=%u prefetch=0 learning=Disabled profile=2 locate=8 tf_hz=20000000 pool_bytes=%u device=DEVICE_PENDING\n",Firmware,ime_device::CandidateArm,ime_device::CandidateOrderFormat,ime_device::CandidateResourceRoot,unsigned(HANIME_N1_ROWS),unsigned(ime_device::CandidateBuildDiagnostics::poolBytes()));
 logLine("[PERF2_CONFIG] firmware=%s core_baseline=HanIME-Core-0.13.1-rc.1 core_api=0.13.0 core_candidate=HN_CORE_PERF2_P3_PREFIX_CONTINUATION_C1_20260923 host_candidate=HN_INPUT_PERF2_P3_CONSUMER_C1_20260923 p2_source=HN_INPUT_PERF2_P2_P_SC_20260923 p2=1 p3=1 prefix_slots=8 planner_allocation=DEFAULT_NEW device=DEVICE_NOT_RUN\n",Firmware);
 logLine("[MEM1_CONFIG] host=%s source=%s lc1=1 py1=1 mr1=%u arrays=4 policy=PSRAM_ONLY backend_bytes=%u record_bytes=%u context_bytes=%u xh_payload=%u py_payload=%u runtime_peak=DEVICE_PENDING\n",hanime_extension::host_candidate_version,hanime_extension::host_candidate_id,unsigned(hanime_extension::host_candidate_mr1),unsigned(s3_mem1_abi[0]),unsigned(s3_mem1_abi[1]),unsigned(s3_mem1_abi[2]),unsigned(s3_mem1_abi[3]),unsigned(s3_mem1_abi[4]));
 logLine("[LC1_CAPACITY] xh_members=%u py_members=%u xh_submission=%u py_submission=%u raw=%u word=%u name=%u text=%u request=%u\n",unsigned(s3_lc1_capacity_abi[0]),unsigned(s3_lc1_capacity_abi[1]),unsigned(s3_lc1_capacity_abi[2]),unsigned(s3_lc1_capacity_abi[3]),unsigned(s3_lc1_capacity_abi[4]),unsigned(s3_lc1_capacity_abi[5]),unsigned(s3_lc1_capacity_abi[6]),unsigned(s3_lc1_capacity_abi[7]),unsigned(s3_lc1_capacity_abi[8]));
 logLine("[NEXT_STAGE_ABI] app=%u raw_viewport=%u file_timing=%u raw_gap_px=%u\n",unsigned(s3_next_stage_abi[0]),unsigned(s3_next_stage_abi[1]),unsigned(s3_next_stage_abi[2]),unsigned((smallFont.glyph(32).advance+1)/2));
 logLine("[IME_ABI] cplusplus=%u exceptions=%u gcc=%u.%u string=%u resources=%u controller=%u window=%u item=%u row=%u view=%u selection=%u attempt=%u font_bytes=%u\n",unsigned(s3_ime_compile_abi[0]),unsigned(s3_ime_compile_abi[1]),unsigned(s3_ime_compile_abi[2]),unsigned(s3_ime_compile_abi[3]),unsigned(s3_ime_compile_abi[4]),unsigned(s3_ime_memory_abi[0]),unsigned(s3_ime_memory_abi[1]),unsigned(s3_ime_memory_abi[2]),unsigned(s3_ime_memory_abi[3]),unsigned(s3_ime_memory_abi[4]),unsigned(s3_ime_memory_abi[5]),unsigned(s3_ime_memory_abi[6]),unsigned(s3_ime_memory_abi[7]),unsigned(s3_ime_memory_abi[8]));
 logLine("[SCOPE] BLE HID HOST / HANIME BASIC / TEST DIRECTORY / NO WIFI\n");
 logLine("[HARDWARE] flash=%u psram=%u pins=DISPLAY(12,11,4,9,10,5),TF(16,17,18,15) power=IN-OUT_BRIDGED\n",unsigned(ESP.getFlashChipSize()),unsigned(ESP.getPsramSize()));
 logLine("[RESET] esp_reason=%u tf_power_control=NONE external_card_power=UNOBSERVED mount_attempt_limit=%u mount_admission_ms=%u single_attempt_ms=%u\n",unsigned(esp_reset_reason()),s3tf::MaximumAttempts,unsigned(s3tf::TotalBudgetMs),unsigned(s3tf::AttemptBudgetMs));
 // Display-before-TF order retained; only the task-owned SD fork has init guards.
 displaySchedule.invalidate();lcd.begin();
 if(!psramFound()||ESP.getPsramSize()!=8388608){fatal="PSRAM unavailable";draw();return;}
#if DRAFTOR_USB_ENABLED
 s3app::preparePlatformMemory();
#endif
 displaySchedule.prepare(lcd.BufferSize);
 logLine("[CORE_CACHE_CONFIG] profile=%u shared_data_kib=%u shared_pages=%u shared_index_limit_kib=8 scheme_data_kib=16 scheme_pages=1 scheme_index_kib=8 order_data_kib=8 order_index_kib=8 output_entries=%u text_bytes=64 form_bytes=64 anchor_slots=%u record_slices=1 Q1=0.8.3\n",ime_device::OutputProfile,ime_device::SharedDataKiB,ime_device::SharedPages,ime_device::OutputEntries,ime_device::SharedAnchorSlots);
 logLine("[ADAPTER_CONFIG] fastseek=%u idf=%u.%u.%u mirror_bytes=%u dirty_interval_ms=33 dirty_pending_limit_ms=50 key_batch_us=2000 nav_capacity=32 file_cache_extra=0\n",FastSeekCompiled,ESP_IDF_VERSION_MAJOR,ESP_IDF_VERSION_MINOR,ESP_IDF_VERSION_PATCH,unsigned(displaySchedule.memoryBytes()));
 memory("before-BLE");
 if(!keyboard::begin()){fatal="BLE task allocation failed";draw();return;}
 BootTfPort tfPort;const auto mount=s3tf::recoverMount(tfPort);
 logLine("[TF_MOUNT_END] ok=%u attempts=%u elapsed_ms=%u max_call_ms=%u over_admission_ms=%u\n",mount.mounted,mount.attempts,unsigned(mount.elapsed_ms),unsigned(mount.max_call_ms),unsigned(mount.elapsed_ms>s3tf::TotalBudgetMs?mount.elapsed_ms-s3tf::TotalBudgetMs:0));
 if(!mount.mounted){tfMountFailed=true;fatal="TF mount failed; bounded retries ended";draw();return;}
 logLine("[TF_CLOCK] init_requested_hz=400000 runtime_requested_hz=%u register_hz=%u (divider; not a scope measurement)\n", unsigned(TfRuntimeHz),unsigned(spiClockDivToFrequency(tfSpi.bus(),tfSpi.getClockDivider())));
 logLine("[TF] official SD.begin mounted; no formatting; root=/HANIME_S3_TEST/v01 max_files=%u resource_active=5 resource_reserved=6 document_bound=3\n",ime_device::Resources::MaximumOpenFiles);storage.noteMounted();

 try{
  auto allocation=draftor::memory::allocate(sizeof(s3app::App));if(!allocation)throw std::bad_alloc();
  std::array<uint8_t,16> uid{};uint64_t efuse=ESP.getEfuseMac();memcpy(uid.data(),&efuse,sizeof(efuse));uint64_t entropy=uint64_t(esp_random())<<32|esp_random();
  app=new(allocation)s3app::App(storage,uid,entropy,font);
  app->setProgressHook([](void*){draw();},nullptr);auto r=app->startup(millis());app->deferFileCommands(true);logLine("[STARTUP] status=%s stage=%s\n",hanime::documents::statusMessage(r.status),r.stage);
  if(!r)fatal="Document startup failed";
  else {
  auto resourceBlock=draftor::memory::allocate(sizeof(ime_device::Resources));if(!resourceBlock)throw std::bad_alloc();
  imeResources=new(resourceBlock)ime_device::Resources(ime_device::CandidateResourceRoot);
   const bool setPrepared=imeResources->checkInstallation();
   logLine("[RESOURCE_INSTALLATION] prepared=%u files=14 auth=compiled_pin demand_auth=1 root=%s error=%s\n",setPrepared,ime_device::CandidateResourceRoot,imeResources->error().c_str());
  const bool fontStarted=font.start(ime_device::CandidateFontPath,imeResources->mount());
  logLine("[FONT] loading=%u bytes=%u read_block=%u storage=PSRAM asset=matched28.hbf1\n",fontStarted,unsigned(ime_device::ChineseFont::Bytes),unsigned(ime_device::ChineseFont::ReadBytes));
  auto block=draftor::memory::allocate(sizeof(ime_device::DeviceIme));if(!block)throw std::bad_alloc();ime=new(block)ime_device::DeviceIme(*app,*imeResources,font,&keyboard::setImeAttempt);}
 }catch(const std::bad_alloc&){if(app&&app->ready()){app->notice="IME allocation failed; English available";app->dirtyDisplay=true;}else fatal="PSRAM allocation failed";logLine("[ERROR] allocation failed; no save acknowledgement\n");}
 logLine("[TRACE_SCHEMA] version=2 mode=1 raw_text=0 event_clock=esp_timer_us nav_release=recorded reset_on_BLE_epoch=1\n");
 logLine("[RESOURCES] independent_ime=API0130_HOST084_RC1_MEM1_PY1_MR1 lexicon=LAZY_TF_READONLY scheme=XIAOHE_DEFAULT font=TF_TO_PSRAM_MATCHED28_U28FCD\n");
 logLine("[MEM_PLAN] app=%u document=%u editor=%u persistence=%u hid_map=%u event=%u\n",unsigned(s3_memory_abi[0]),unsigned(s3_memory_abi[1]),unsigned(s3_memory_abi[2]),unsigned(s3_memory_abi[3]),unsigned(s3_memory_abi[4]),unsigned(s3_memory_abi[5]));
 s3app::stability::capture(s3app::stability::Kind::Ready);
#if DRAFTOR_USB_ENABLED
 {s3app::S3WifiTransport measurement;measurement.measure("boot_off");}
#endif
 memory("ready");if(app)presentContext();else draw();
 BT_MEASURE(setup());
#if DRAFTOR_TELEMETRY_PROFILE
 if(app&&ime&&app->ready()){
  void* observer=heap_caps_malloc(ime_device::behavior::storageBytes(),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  // This one boot diagnostic precedes all behavior frames. Allocation failure
  // leaves the product running and is never hidden by an internal-RAM fallback.
  const std::uint64_t boot=(std::uint64_t(esp_random())<<32)|esp_random();
  Serial.printf("[BT_BOOT] boot=%08x%08x profile=%u observer_bytes=%u allocation=%u reset=%u internal_free=%u psram_free=%u stack_hwm=%u\n",unsigned((boot?boot:1)>>32),unsigned(boot?boot:1),DRAFTOR_TELEMETRY_PROFILE,unsigned(ime_device::behavior::storageBytes()),observer!=nullptr,unsigned(esp_reset_reason()),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  ime_device::behavior::bind(observer,boot?boot:1,*app,ime);
 }
#endif
}
#if DRAFTOR_CP_OBSERVE
// One existing-queue summary after each completed activation, no per-slice log.
// No inline keeps this temporary snapshot out of the main loop's fixed frame.
__attribute__((noinline)) static void observeBlockActivation(uint64_t ready_us){
 if(!diagnostics||!imeResources)return;
 const auto snap=imeResources->orderSnapshot();
 uint64_t allocations=0,init_calls=0,local_work=0;unsigned nav=0;
#if defined(DRAFTOR_BLOCK_BACKEND) && DRAFTOR_BLOCK_BACKEND
 if(const auto order=imeResources->scheme.base_order()){
  const auto stats=hanime::detail::BaseOrderBlockAccess::initialization_stats(*order);
  nav=unsigned(stats.nav_bytes);allocations=stats.allocations;init_calls=stats.initialization_calls;local_work=stats.local_work;
 }
#endif
 using U=unsigned long long;
 diagnostics->output.printf("[B_ACTIVATE] request=%u start=%llu ready=%llu scheme=%u calls=%llu bytes=%llu work=%llu data=%llu proof=%llu logical=%llu logical_bytes=%llu nav=%u allocations=%llu init_calls=%llu local_work=%llu queue_dropped=%llu\n",
  unsigned(imeTiming.serial),U(imeTiming.request_us),U(ready_us),unsigned(imeResources->rules()),U(snap.cost.read_calls),U(snap.cost.read_bytes),U(snap.cost.work_items),U(snap.authenticated_loads),U(snap.authenticated_index_loads),U(snap.logical_requests),U(snap.logical_bytes),nav,U(allocations),U(init_calls),U(local_work),U(diagnostics->output.costs().dropped));
}
#endif
void deviceLoop(){
 if(heapIntegrityStopped){heapIntegrityStopTurn();return;}
 uint32_t start=micros();auto ble=keyboard::status();
 serialDiagnostic();
 if(ime&&!usbActive()&&!wifiBlocked())ime->serviceVisuals(millis());
 auto observeEpoch=[&](uint32_t current){if(lastEpoch!=current){s3app::stability::capture(s3app::stability::Kind::BleBoundary,lastEpoch,current,unsigned(ble.phase));ime_device::uiTrace.setMode(1);detailedDiagnostics=false;lastEpoch=current;repeat.clear();if(ime)ime->interrupt();if(app)app->disconnected();}};
 observeEpoch(ble.epoch);
 if(uint32_t(ble.phase)!=lastBleState||ble.passkey!=lastPasskey||ble.pairing_not_saved!=lastPairingWarning){s3app::stability::event(s3app::stability::Kind::BlePhase,lastBleState,unsigned(ble.phase),ble.epoch);lastPairingWarning=ble.pairing_not_saved;lastBleState=uint32_t(ble.phase);lastPasskey=ble.passkey;if(app)app->dirtyDisplay=true;}
 if(app&&!fatal&&app->ready()){
  try{
   presentContext(); // publish reconnect/focus boundary before accepting queued keys
   auto repeatContext=[&](){repeat.queryAxis(app->findOpen(),!app->horizontalActive());repeat.context(app->focusToken(),!usbActive()&&!wifiBlocked()&&(!ime||!ime->chinese()||!ime->active())&&(app->renameOpen()||(app->session.hasCurrent()&&!app->filesOpen()&&!app->statsOpen()))&&!app->commandBarrier()&&app->manager.state().pending==hanime::documents::PendingAction::None);};
   repeatContext();s3hid::Event e;unsigned processed=0;const auto keyBatchStart=micros();
   while(!app->commandBarrier()&&processed<64&&(!processed||micros()-keyBatchStart<2000)&&keyboard::pop(e)){
    ++processed;
    // Keep the original cumulative age metric, including rejected events.
    maxInputAge=std::max(maxInputAge,millis()-e.ms);
    uint32_t currentEpoch=0;const bool admitted=keyboard::admit(e,currentEpoch);
    observeEpoch(currentEpoch);
    if(!admitted){ime_device::traceUi(ime_device::UiTraceKind::RouteDrop,ime?ime->composition():nullptr,ime?ime->window():nullptr,&e,nullptr,uint8_t(e.down));continue;}
#if DRAFTOR_USB_ENABLED
    if((usbActive()||wifiBlocked())&&(!e.down||e.usage>=224)){
     // The existing owners see releases/modifiers even across modal route epochs.
     productInput.layout(*app,e);productInput.ime(*app,ime,e);repeat.clear();continue;
    }
#endif
    if(!inputRoute.accept(e,millis())){
     // Retire only same-epoch Enter/F2 releases across the completed frame;
     // the rejected event still cannot act on this UI or reach app->key().
     productInput.releaseAfterRouteDrop(*app,ime,e);
     ime_device::traceUi(ime_device::UiTraceKind::RouteDrop,ime?ime->composition():nullptr,ime?ime->window():nullptr,&e,nullptr,uint8_t(e.down));continue;
    }
#if DRAFTOR_USB_ENABLED
    if(wifiKey(e)){repeat.clear();presentContext();continue;}
    if(wifiRuntime&&s3app::usbEntryKey(*app,e)){app->notice="Turn Wi-Fi off before USB transfer";app->dirtyDisplay=true;continue;}
    if(usbKey(e)){if(heapIntegrityStopped){heapIntegrityStopTurn();return;}repeat.clear();presentContext();continue;}
#endif
    // Layout is a product action. It never enters DeviceIme, even while
    // query/selection/submission is Working. Candidate route tokens stay intact.
    if(productInput.layout(*app,e)){repeat.clear();continue;}
    const bool bodyInput=e.down&&e.usage<224&&app->session.hasCurrent()&&!app->filesOpen()&&!app->findOpen()&&!app->statsOpen();
    if(bodyInput){++response.accepted;response.max_age_ms=std::max(response.max_age_ms,millis()-e.ms);}
    const auto oldRevision=app->sharedState().document.revision();
    const bool wasChinese=ime&&ime->chinese(),hadRaw=ime&&!ime->raw().empty();
    std::array<char,65> oldRaw{};const auto oldRawSize=ime?ime->raw().size():0;if(ime)std::copy(ime->raw().begin(),ime->raw().end(),oldRaw.begin());
    const auto eventUs=ime_device::receivedUs(ime_device::clockUs(),millis(),e.ms);const auto oldScheme=imeResources?imeResources->rules():hanime_extension::SchemeRules::R1Xiaohe;
    auto keyStart=micros();if(!productInput.ime(*app,ime,e)){
     const auto oldRequest=app->fileTiming.serial;const auto dispatchUs=ime_device::clockUs();repeat.event(e);app->key(e,wasChinese);
     if(app->fileTiming.serial!=oldRequest)logLine("[FILE_INPUT] request=%u action=%u epoch=%u route=%u event_ms=%u received_us=%llu dispatch_us=%llu\n",unsigned(app->fileTiming.serial),unsigned(ime?ime->actionId():0),unsigned(e.epoch),unsigned(e.route_epoch),unsigned(e.ms),(unsigned long long)eventUs,(unsigned long long)dispatchUs);
    }else repeat.clear();if(ime)keyboard::publishImeContext(ime->ingressContext(),ime->resourceOwner());
    if(ime&&imeResources){
     if((!wasChinese&&ime->chinese())||oldScheme!=imeResources->rules()){
      imeTiming.request_us=eventUs;++imeTiming.serial;imeTiming.ready_pending=true;
      logLine("[IME_LOAD_REQUEST] request=%u uptime_us=%llu reason=%s scheme=%s\n",unsigned(imeTiming.serial),(unsigned long long)imeTiming.request_us,oldScheme!=imeResources->rules()?"switch":"enter_cn",imeResources->rules()==hanime_extension::SchemeRules::R1Xiaohe?"XH":"PY");
     }
     if(!ime->raw().empty()&&(oldRawSize!=ime->raw().size()||!std::equal(ime->raw().begin(),ime->raw().end(),oldRaw.begin()))){
      if(!hadRaw){imeTiming.first_raw_us=eventUs;++imeTiming.raw_serial;}
      imeTiming.last_raw_us=eventUs;imeTiming.frame_pending=true;
     }
     if(wasChinese!=ime->chinese())s3app::stability::event(s3app::stability::Kind::ImeMode,unsigned(wasChinese),unsigned(ime->chinese()),unsigned(imeResources->state()));
     if(wasChinese!=ime->chinese())logLine("[IME_MODE] uptime_us=%llu mode=%s stage=%u raw=%u\n",(unsigned long long)ime_device::clockUs(),ime->chinese()?"CN":"EN",unsigned(imeResources->state()),unsigned(ime->raw().size()));
     if(ime->raw().empty())imeTiming.frame_pending=false;
    }
    if(bodyInput){response.key_us=std::max(response.key_us,micros()-keyStart);if(app->sharedState().document.revision()!=oldRevision&&!response.awaiting){response.awaiting=true;response.pending_ms=e.ms;response.pending_focus=app->focusToken();}}if(s3profile::active)s3profile::record.max_main_step_us=std::max(s3profile::record.max_main_step_us,uint64_t(micros()-keyStart));presentContext();repeatContext();
   }
   ble=keyboard::status();observeEpoch(ble.epoch);
   if(!usbActive()&&!wifiBlocked()&&!app->commandBarrier()&&!keyboard::pending()&&ble.phase==keyboard::Phase::Ready&&repeat.poll(millis(),ble.epoch,e)){
    uint32_t currentEpoch=0;const bool admitted=keyboard::admit(e,currentEpoch);observeEpoch(currentEpoch);
    if(admitted){auto before=app->editStamp();auto nameBefore=app->renameRevision();auto queryBefore=app->queryStamp();app->key(e);auto after=app->editStamp();repeat.actionResult(queryBefore.revision!=app->queryStamp().revision||queryBefore.cursor!=app->queryStamp().cursor||queryBefore.selected!=app->queryStamp().selected||app->bodyNavigationPending()||nameBefore!=app->renameRevision()||before.revision!=after.revision||before.selection!=after.selection);}
   }
   if(tfProbe&&ime&&ime->chinese())tfProbe->cancel();
   const bool saving=app->persistence.backgroundSaveActive();ime_device::ioProfile.save_active=saving;
   uint32_t serviceStart=micros();const bool deferSave=keyboard::pending()&&millis()-lastSaveTurn<20;
#if DRAFTOR_USB_ENABLED
   if(usbActive()){const auto before=usbRuntime->transfer.phase();usbRuntime->transfer.tick(millis(),usbInput());if(before!=usbRuntime->transfer.phase())s3app::stability::capture(s3app::stability::Kind::UsbPhase,unsigned(before),unsigned(usbRuntime->transfer.phase()));}
   else
#endif
   if(!wifiBlocked())app->service(millis(),deferSave);
#if DRAFTOR_USB_ENABLED
   if(wifiRuntime){wifiRuntime->transfer.tick(millis(),usbInput());releaseWifiRuntime();}
#endif
   ime_device::ioProfile.save_active=app->persistence.backgroundSaveActive();
   if(!deferSave){lastSaveTurn=millis();++initSchedule.save_turns;}else ++initSchedule.save_deferred;
   if(saving)response.save_us=std::max(response.save_us,micros()-serviceStart);
   if(keyboard::pending())++initSchedule.blocked_by_keys;
   else if(!usbActive()&&!wifiBlocked()){
    const bool initializing=(!font.ready()&&!font.failed())||(imeResources&&!imeResources->ready()&&imeResources->state()!=ime_device::ResourceState::Error&&imeResources->state()!=ime_device::ResourceState::Dormant);
    const auto quantum=initializing?InitQuantumUs:RunQuantumUs;const auto limit=initializing?InitSlices:RunSlices;
    const auto workStart=micros();unsigned slices=0;++initSchedule.turns;
    CPATTR(cpattr::windowBegin());
    do{
     const auto sliceStart=micros();
     if(!font.ready()&&!font.failed())font.service();else if(tfProbe)tfProbe->service();else if(ime){
      if(ime->window())ime->window()->setPrefetch(rowPrefetchEnabled);
      ime->service(rowPrefetchEnabled&&!keyboard::pending()&&!app->persistence.backgroundSaveActive());
      keyboard::publishImeContext(ime->ingressContext(),ime->resourceOwner());
     }
     initSchedule.max_slice_us=std::max(initSchedule.max_slice_us,micros()-sliceStart);++slices;
    }while(
#if DRAFTOR_CP_OBSERVE
    cpattr::again(slices,limit,quantum,[&]{return micros()-workStart;},[]{return keyboard::pending();},[]{return ime_device::candidateWorkShouldYield(ime,app);},[]{return (!font.ready()&&!font.failed())||(tfProbe&&!tfProbe->hasResult()&&!tfProbe->finished())||(!tfProbe&&ime&&ime->hasWork());})
#else
    slices<limit&&micros()-workStart<quantum&&!keyboard::pending()&&!ime_device::candidateWorkShouldYield(ime,app)&&((!font.ready()&&!font.failed())||(tfProbe&&!tfProbe->hasResult()&&!tfProbe->finished())||(!tfProbe&&ime&&ime->hasWork()))
#endif
    );
    CPATTR(cpattr::windowEnd());
    initSchedule.slices+=slices;initSchedule.max_turn_us=std::max(initSchedule.max_turn_us,micros()-workStart);
   }
   BT_MEASURE(sample(ime_device::bench::ServiceTurn,micros()-serviceStart));
   maxService=std::max(maxService,micros()-serviceStart);if(s3profile::active)s3profile::record.max_main_step_us=std::max(s3profile::record.max_main_step_us,uint64_t(micros()-serviceStart));presentContext();repeatContext();
   if(app->operationSerial!=lastOperation){
    const auto logStart=micros();lastOperation=app->operationSerial;
    if(diagnostics){
     auto& d=*diagnostics;
     if(d.document_pending||d.create_row<10)++d.superseded;
     d.operation=lastOperation;d.operation_us=ime_device::clockUs();
     d.operation_status=hanime::documents::statusMessage(app->lastOperation.status);
     const char* stage=app->lastOperation.stage?app->lastOperation.stage:"";
     if(std::strlen(stage)>=d.operation_stage.size()){stage="stage_name_over_capacity";++d.superseded;}
     std::strcpy(d.operation_stage.data(),stage);
     d.body_committed=app->lastOperation.body_committed;d.dirty=app->sharedState().document.dirty();
     d.body_bytes=app->sharedState().document.text().size();d.cursor=app->sharedState().document.cursorByte();
     d.document_pending=true;d.create=app->createTiming;d.create_row=d.create.steps[1]?1:10;
    }
    app->createTiming={};memory("document-operation");
    response.log_us=std::max(response.log_us,micros()-logStart);
   }
  }catch(const std::bad_alloc&){if(ime)ime->interrupt();app->allocationFailed();repeat.clear();logLine("[ERROR] allocation failed; pending operation NOT acknowledged\n");}
 }
 if(reportedDiscards!=inputRoute.discarded&&!keyboard::pending()){
  logLine("[INPUT_CONTEXT] discarded_total=%u max_discarded_age_ms=%u policy=WAIT_FOR_DESTINATION_FRAME\n",unsigned(inputRoute.discarded),unsigned(inputRoute.max_discarded_age_ms));
  reportedDiscards=inputRoute.discarded;
 }
 finishProbe();
 if(imeResources){
  if(imeResources->state()==ime_device::ResourceState::Error&&!resourceErrorReported){
   logLine("[RESOURCE_ERROR] uptime_us=%llu detail=%s\n",(unsigned long long)ime_device::clockUs(),imeResources->error().c_str());resourceErrorReported=true;
  }
  const auto next=ime_device::loadSnapshot(font,*imeResources);const auto oldStage=loadingStatus.shown().stage;
  if(loadingStatus.update(next,millis())){
   if(app)app->dirtyDisplay=true;
   if(next.stage!=oldStage)logLine("[LOAD_STAGE] uptime_us=%llu stage=%u done=%llu total=%llu text=%s\n",(unsigned long long)ime_device::clockUs(),unsigned(next.stage),(unsigned long long)next.done,(unsigned long long)next.total,next.text);
  }
  if(imeTiming.ready_pending&&font.ready()&&imeResources->ready()){
   const auto now=ime_device::clockUs();
#if DRAFTOR_CP_OBSERVE
   observeBlockActivation(now);
#endif
   logLine("[IME_READY] request=%u uptime_us=%llu from_request_us=%llu from_boot_log_us=%llu scope=published_release_required_blocks diagnostic=%u\n",unsigned(imeTiming.serial),(unsigned long long)now,(unsigned long long)(now-imeTiming.request_us),(unsigned long long)(now-bootLogUs),probeUsedThisBoot);s3app::stability::event(s3app::stability::Kind::ImeReady,imeTiming.serial,unsigned(imeResources->state()),unsigned(ime&&ime->chinese()));imeTiming.ready_pending=false;memory("ime-ready");
  }
  if(ime&&ime->raw().empty())imeTiming.frame_pending=false;
 }
 const bool candidateReady=ime_device::candidatePublicationPending(ime,app);
 const bool acceptedBody=ime&&ime->acceptedAction()!=lastBodyFrameAction&&ime->acceptedDocumentCurrent();
 if(displaySchedule.due(millis(),!app||app->dirtyDisplay||fatal,keyboard::pending(),candidateReady||acceptedBody||(app&&app->presentationPending())))draw();
 if(millis()-lastMemory>15000){lastMemory=millis();requestSummary();}
 idleDiagnostics();
#if DRAFTOR_TELEMETRY_CAPTURE
 const auto behavior_service_start=ime_device::clockUs();
#endif
 s3app::stability::periodic(millis(),unsigned(ime&&ime->chinese())|(unsigned(ime&&ime->active())<<1)|(unsigned(usbActive())<<2)|(unsigned(app&&app->persistence.backgroundSaveActive())<<3));
 bool stabilityOwnsUart=false;
#if DRAFTOR_TELEMETRY_PROFILE
 if(!behaviorSink.pending())stabilityOwnsUart=s3app::stability::service(keyboard::pending());
#else
#if DRAFTOR_AUTH_OBSERVE
 if(!diagnostics||!diagnostics->output.partial())
#endif
 stabilityOwnsUart=s3app::stability::service(keyboard::pending());
#endif
 if(!stabilityOwnsUart){
#if DRAFTOR_AUTH_OBSERVE
 if(diagnostics)diagnostics->output.service(serialLogPort,logGate());
#endif
 BT_OBSERVE(service(behaviorSink,keyboard::pending()));
 BT_MEASURE(sample(ime_device::bench::SerialDrain,ime_device::clockUs()-behavior_service_start));
#if DRAFTOR_TELEMETRY_PROFILE
 BT_MEASURE(service(ime_device::behavior::drained()&&!behaviorSink.pending(),displaySchedule.sent,displaySchedule.reused,displaySchedule.failed));
#else
 BT_MEASURE(service(true,displaySchedule.sent,displaySchedule.reused,displaySchedule.failed));
#endif
 }
 BT_MEASURE(sample(ime_device::bench::Loop,micros()-start));
 maxLoop=std::max(maxLoop,micros()-start);
 const bool foreground=ime&&ime->hasWork();
#if DRAFTOR_CP_OBSERVE
 const auto cp_delay=loopYield.delayMs(ime_device::clockUs(),foreground);const auto cp_delay_begin=ime_device::clockUs();
 delay(cp_delay);cpattr::yielded(cp_delay,cp_delay_begin,ime_device::clockUs());
#else
 delay(loopYield.delayMs(ime_device::clockUs(),foreground));
#endif
}
