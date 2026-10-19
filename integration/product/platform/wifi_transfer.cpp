// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#include "wifi_transfer.hpp"
#include "wifi_page.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace s3app {
namespace {
bool equalCase(std::string_view a,std::string_view b){
 if(a.size()!=b.size())return false;
 for(std::size_t i=0;i<a.size();++i){auto c=a[i];if(c>='A'&&c<='Z')c+=32;if(c!=b[i])return false;}return true;
}
std::string_view trim(std::string_view s){while(!s.empty()&&(s.front()==' '||s.front()=='\t'))s.remove_prefix(1);while(!s.empty()&&(s.back()==' '||s.back()=='\t'))s.remove_suffix(1);return s;}
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool number(std::string_view s,std::uint64_t& n,unsigned base){
 n=0;if(s.empty()||s.size()>20)return false;
 for(char c:s){int d=hex(c);if(d<0||unsigned(d)>=base||n>(UINT64_MAX-unsigned(d))/base)return false;n=n*base+unsigned(d);}return true;
}
std::string_view param(std::string_view uri,std::string_view name){
 auto q=uri.find('?');if(q==uri.npos)return {};uri.remove_prefix(q+1);
 while(!uri.empty()){const auto end=uri.find('&');const auto p=uri.substr(0,end);const auto e=p.find('=');if(e!=p.npos&&p.substr(0,e)==name)return p.substr(e+1);if(end==uri.npos)break;uri.remove_prefix(end+1);}return {};
}
bool filename(std::string_view s,std::array<char,256>& name){
 std::size_t n=0;
 for(std::size_t i=0;i<s.size();++i){unsigned char c=s[i];if(c=='%'){if(i+2>=s.size()||hex(s[i+1])<0||hex(s[i+2])<0)return false;c=unsigned(hex(s[i+1])*16+hex(s[i+2]));i+=2;}
  if(c<32||c==127||c=='/'||c=='\\'||c==':'||n>=name.size()-1)return false;
  name[n++]=char(c);
 }
 name[n]=0;return n>4&&name[0]!='.'&&equalCase({name.data()+n-4,4},".txt");
}
unsigned httpStatus(wx::Status s){switch(s){case wx::Status::Busy:case wx::Status::Conflict:return 409;case wx::Status::TooLarge:return 413;case wx::Status::InvalidText:return 422;case wx::Status::Full:case wx::Status::Exhausted:return 507;case wx::Status::NotFound:return 404;case wx::Status::Stale:return 409;default:return 500;}}
}
WifiTransfer::WifiTransfer(App& app,WifiTransport& net,UsbDocumentImport& sink)noexcept
 :app_(app),net_(net),sink_(sink),qr_(net),owner_(app),snapshot_(owner_,net,{65536,1024}){}
void WifiTransfer::showQr(WifiQrPage page)noexcept{
 qr_.show(page,ssid_,password_,net_.address());app_.dirtyDisplay=true;
}
bool WifiTransfer::enterGate(TransferInputState input)noexcept{
 if(input.composition||input.semantic_pending||input.pending_body_edits||app_.persistence.backgroundSaveActive()||app_.session.busy())return false;
 if(panel_)return true;
 if(gated_)return false;
 gated_=app_.enterTransfer();return gated_;
}
void WifiTransfer::leaveGate()noexcept{if(gated_&&!panel_&&!import_gate_){app_.leaveTransfer();gated_=false;}}
bool WifiTransfer::start(TransferInputState input)noexcept{
 if(active())return true;
 if(!enterGate(input)){message_="Wi-Fi unavailable; finish current action";return false;}
 panel_=true;net_.measure("before_start");
 qr_.clear();
 if(!net_.start(ssid_,sizeof(ssid_),password_,sizeof(password_),token_,sizeof(token_))){
  message_=net_.startError();net_.stop();panel_=false;leaveGate();
  draftor::qr::erase(ssid_,sizeof(ssid_));draftor::qr::erase(password_,sizeof(password_));draftor::qr::erase(token_,sizeof(token_));return false;
 }
 phase_=WifiPhase::Listening;message_="Wi-Fi on; Esc: write, O: off";showQr(WifiQrPage::Connect);net_.measure("on");return true;
}
void WifiTransfer::abortImport()noexcept{
 if(operation_&&import_gate_){sink_.abort(operation_);if(publishing_){last_import_.operation=operation_;last_import_.status=wx::Status::Uncertain;message_="Import uncertain; query operation";}else message_="Import cancelled; original retained";}
 import_gate_=false;publishing_=false;leaveGate();
}
void WifiTransfer::finishConnection()noexcept{
 net_.close();
 if(lease_.token.generation){snapshot_.release(lease_.token);lease_={};net_.measure("export_released");}
 snapshot_.stop();
 if(import_gate_){abortImport();net_.measure("import_released");}
 header_bytes_=reply_bytes_=reply_at_=payload_bytes_=payload_at_=remaining_=body_bytes_=0;payload_=nullptr;operation_=0;
 if(active())phase_=WifiPhase::Listening;
}
void WifiTransfer::stop()noexcept{
 qr_.clear();
 if(!active())return;
 finishConnection();net_.stop();phase_=WifiPhase::Off;panel_=false;leaveGate();
 draftor::qr::erase(ssid_,sizeof(ssid_));draftor::qr::erase(password_,sizeof(password_));draftor::qr::erase(token_,sizeof(token_));message_="Wi-Fi off";app_.notice=message_;app_.dirtyDisplay=true;net_.measure("off_after");
}
bool WifiTransfer::key(const s3hid::Event& e,TransferInputState input)noexcept{
 if(!active()){if(!wifiEntryKey(app_,e))return false;if(!start(input)){app_.notice=message_;app_.dirtyDisplay=true;}return true;}
 if(panel_){
  if(e.down&&!e.mods){
   if(e.usage==18){stop();return true;}
   if(e.usage==41){qr_.clear();panel_=false;leaveGate();app_.dirtyDisplay=true;}
   else if(e.usage==43)showQr(qr_.page()==WifiQrPage::Connect?WifiQrPage::Address:WifiQrPage::Connect);
   else if(e.usage==19)showQr(qr_.page()==WifiQrPage::Manual?WifiQrPage::Connect:WifiQrPage::Manual);
  }
  return true;
 }
 if(import_gate_){
  // Keep the explicit stop route available during a slow upload/TF write.
  // The import already owns the App gate, so opening this panel needs no
  // second ownership transition and O can abort/reconcile that operation.
  if(wifiEntryKey(app_,e)){panel_=true;showQr(WifiQrPage::Connect);}
  return true;
 }
 if(wifiEntryKey(app_,e)){if(enterGate(input)){panel_=true;showQr(WifiQrPage::Connect);}return true;}return false;
}
void WifiTransfer::response(unsigned code,const char* type,const char* data,std::size_t size,const char* extra)noexcept{
 const int n=std::snprintf(reply_,sizeof(reply_),"HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'\r\n%s\r\n",code,code<400?"OK":"Error",type,unsigned(size),extra);
 if(n<0||std::size_t(n)>=sizeof(reply_)){finishConnection();return;}
 reply_bytes_=std::size_t(n);reply_at_=payload_at_=0;payload_=data;payload_bytes_=size;phase_=WifiPhase::Send;progress_=now_;
}
void WifiTransfer::error(unsigned code,const char* message)noexcept{
 abortImport();std::snprintf(body_,sizeof(body_),"{\"imported\":false,\"message\":\"%s\"}",message);
 response(code,"application/json; charset=utf-8",body_,std::strlen(body_));
}
bool WifiTransfer::capture(TransferInputState input)noexcept{
 snapshot_status_=wx::SessionError::Busy;
 // A synchronous owner turn already excludes editor mutations. Do not enter
 // the modal App transfer route here: changing focus would drop queued BLE
 // keystrokes even though the user continues editing the same document.
 if(input.composition||input.semantic_pending||input.pending_body_edits||
    (app_.transferActive()&&!panel_)||app_.commandBarrier()||app_.session.busy())return false;
 owner_.observe(now_,input);
 auto result=snapshot_.begin();
 snapshot_status_=result;
 if(result==wx::SessionError::None){
  // One serialized product turn. At most 64 copy chunks plus state transitions;
  // no network I/O, storage writes or event pumping while this gate is held.
  for(unsigned i=0;i<68&&(snapshot_.phase()==wx::Phase::Saving||snapshot_.phase()==wx::Phase::Copying);++i)snapshot_.tick();
 }
 const bool ok=snapshot_.acquire(lease_)==wx::ReaderStatus::Ok;
 if(!ok&&result==wx::SessionError::None)snapshot_status_=snapshot_.error();
 snapshot_.stop();owner_.unfreeze();
 if(ok)net_.measure("export_snapshot");
 return ok;
}
void WifiTransfer::imported(wx::Status status)noexcept{
 if(status==wx::Status::Ok){wx::ImportResult r;status=sink_.query(operation_,r);if(status==wx::Status::Ok)last_import_=r;}
 if(status!=wx::Status::Ok||last_import_.outcome!=wx::Outcome::Imported){
  sink_.abort(operation_);import_gate_=false;leaveGate();publishing_=false;
  last_import_.operation=operation_;last_import_.status=wx::Status::Uncertain;
  error(409,"Publication uncertain. Query this operation before retrying.");return;
 }
 import_gate_=false;publishing_=false;leaveGate();
 char id[33]{};for(unsigned i=0;i<16;++i)std::snprintf(id+i*2,3,"%02x",last_import_.document[i]);
 std::snprintf(body_,sizeof(body_),"{\"imported\":true,\"operation\":\"%016llx\",\"document\":\"%s\",\"bytes\":%u,\"message\":\"New document created. CURRENT unchanged.\"}",static_cast<unsigned long long>(last_import_.operation),id,unsigned(last_import_.bodyBytes));
 message_="New draft imported; CURRENT unchanged";app_.notice=message_;app_.dirtyDisplay=true;net_.measure("import_committed");
 response(200,"application/json; charset=utf-8",body_,std::strlen(body_));
}
void WifiTransfer::request(TransferInputState input)noexcept{
 std::string_view h(headers_,header_bytes_);auto end=h.find("\r\n");if(end==h.npos){error(400,"Invalid HTTP request");return;}
 const auto first=h.substr(0,end);auto a=first.find(' '),b=first.rfind(' ');
 if(a==first.npos||a==b||first.substr(b+1)!="HTTP/1.1"){error(400,"HTTP/1.1 required");return;}
 const auto method=first.substr(0,a),uri=first.substr(a+1,b-a-1),path=uri.substr(0,uri.find('?'));
 bool haveLength=false,host=false;std::uint64_t length=0;std::string_view auth,origin,contentType;
 const std::string_view service=net_.address();
 const auto authority=service.substr(7); // Transport publishes an http:// IPv4 service URL.
 h.remove_prefix(end+2);
 while(h.size()>2){end=h.find("\r\n");if(end==h.npos)break;auto line=h.substr(0,end);if(line.empty())break;auto colon=line.find(':');
  if(colon==line.npos||line.front()==' '||line.front()=='\t'){error(400,"Invalid header");return;}
  auto name=line.substr(0,colon),value=trim(line.substr(colon+1));
  if(equalCase(name,"content-length")){if(haveLength||!number(value,length,10)){error(400,"Invalid Content-Length");return;}haveLength=true;}
  else if(equalCase(name,"transfer-encoding")||equalCase(name,"expect")){error(400,"Unsupported transfer encoding or expectation");return;}
  else if(equalCase(name,"host")){if(host||(value!=authority&&!(authority.find(':')==authority.npos&&value.substr(0,authority.size())==authority&&value.substr(authority.size())==":80"))){error(403,"Use the device transfer address");return;}host=true;}
  else if(equalCase(name,"x-draftor-token")){if(!auth.empty()){error(400,"Duplicate token");return;}auth=value;}
  else if(equalCase(name,"origin"))origin=value;
  else if(equalCase(name,"content-type"))contentType=value;
  h.remove_prefix(end+2);
 }
 if(!host||(!origin.empty()&&origin!=service)){error(403,"Same-origin access required");return;}
 if(method=="GET"&&length){error(400,"GET body is unsupported");return;}
 if(method=="GET"&&path=="/"){response(200,"text/html; charset=utf-8",WifiPage,sizeof(WifiPage)-1);return;}
 if(method=="GET"&&path=="/session"){std::snprintf(body_,sizeof(body_),"{\"token\":\"%s\",\"bodyLimit\":65536}",token_);response(200,"application/json",body_,std::strlen(body_));return;}
 if(auth.empty())auth=param(uri,"token");
 if(auth!=token_){error(403,"Session expired. Reload this page.");return;}
 if(method=="GET"&&path=="/export"){
  if(!capture(input)){
   if(snapshot_status_==wx::SessionError::AllocationFailed)error(507,"Not enough PSRAM for export snapshot");
   else if(snapshot_status_==wx::SessionError::SourceTooLarge)error(413,"Current document exceeds 65536 bytes");
   else if(snapshot_status_==wx::SessionError::Text)error(422,"Current document is not valid exportable UTF-8");
   else error(409,"Snapshot unavailable. Finish composition or current operation, then retry.");
   return;
  }
  response(200,"text/plain; charset=utf-8",lease_.data,lease_.size,"Content-Disposition: attachment; filename=\"DOCUMENT.TXT\"\r\n");return;
 }
 if(method=="POST"&&path=="/ticket"){
  if(length||!enterGate(input)){error(409,"Finish current operation before importing");return;}
  std::uint64_t op=0;const auto s=sink_.allocateOperation(op);leaveGate();if(s!=wx::Status::Ok){error(httpStatus(s),"Storage unavailable for import");return;}
  std::snprintf(body_,sizeof(body_),"{\"operation\":\"%016llx\"}",static_cast<unsigned long long>(op));response(200,"application/json",body_,std::strlen(body_));return;
 }
 if(method=="GET"&&path=="/receipt"){
  if(!number(param(uri,"op"),operation_,16)||!operation_){error(400,"Invalid operation");return;}
  if(!enterGate(input)){error(409,"Finish current operation before querying");return;}
  wx::ImportResult r;const auto s=sink_.query(operation_,r);
  if(s==wx::Status::Ok&&r.outcome==wx::Outcome::Imported){import_gate_=true;last_import_=r;imported(wx::Status::Ok);return;}
  leaveGate();
  if((s==wx::Status::Ok&&r.outcome==wx::Outcome::Aborted)||s==wx::Status::NotFound){
   std::snprintf(body_,sizeof(body_),"{\"imported\":false,\"retrySafe\":true,\"message\":\"No published document for this operation. Select the file again to retry.\"}");response(200,"application/json",body_,std::strlen(body_));return;
  }
  error(409,"Import not confirmed. Keep operation ID and check document list before retrying.");return;
 }
 if(method!="POST"||path!="/import"){error(404,"Unknown endpoint");return;}
 if(!haveLength){error(411,"Content-Length required");return;}
 if(length>131075){error(413,"File exceeds wire limit 131075 bytes; body limit is 65536 bytes");return;}
 if(contentType!="application/octet-stream"){error(415,"Upload a raw UTF-8 TXT file");return;}
 wx::ImportIntent intent;
 if(!number(param(uri,"op"),intent.operation,16)||!intent.operation||!filename(param(uri,"name"),intent.name)){error(400,"Invalid operation or TXT filename");return;}
 if(!enterGate(input)){error(409,"Finish composition or current file action before importing");return;}
 import_gate_=true;operation_=intent.operation;last_import_={};last_import_.operation=operation_;
 // Stable operation-to-request identity survives connection loss and AP restart.
 // Filename is metadata only and never a filesystem path.
 intent.source={intent.operation,1,length};wx::ImportResult receipt;
 auto s=sink_.reserve(intent,receipt);
 if(s==wx::Status::Ok&&receipt.outcome==wx::Outcome::Imported){last_import_=receipt;imported(s);return;}
 if(s==wx::Status::Ok)s=sink_.begin(operation_);
 if(s!=wx::Status::Ok){error(httpStatus(s),"Import reservation failed; CURRENT unchanged");return;}
 text_=wx::ImportWire(65536);body_bytes_=0;remaining_=std::size_t(length);phase_=WifiPhase::Upload;
 message_="Receiving TXT; editing resumes shortly";app_.notice=message_;app_.dirtyDisplay=true;net_.measure("import_buffer");
}
bool WifiTransfer::append(const wx::Step& step)noexcept{
 if(step.flow==wx::Flow::Error){
  const auto code=step.failure.code;const char* reason="Invalid UTF-8";
  if(code==wx::TextError::Nul)reason="NUL is not allowed";
  else if(code==wx::TextError::TruncatedUtf8)reason="Incomplete UTF-8 character at end of file";
  else if(code==wx::TextError::BodyLimit)reason="Normalized body exceeds 65536 bytes";
  else if(code==wx::TextError::InputLimit)reason="Raw upload exceeds 131075 bytes";
  char message[160];std::snprintf(message,sizeof(message),"%s at source byte %u; import aborted",reason,unsigned(step.failure.offset));
  error(code==wx::TextError::BodyLimit||code==wx::TextError::InputLimit?413:422,message);return false;
 }
 if(step.produced){const auto s=sink_.append(operation_,{output_,step.produced});if(s.status!=wx::Status::Ok||s.bytes!=step.produced){error(httpStatus(s.status),"Storage write failed; import aborted");return false;}body_bytes_+=step.produced;}return true;
}
void WifiTransfer::tick(std::uint32_t now,TransferInputState input)noexcept{
 now_=now;if(!active())return;
 if(phase_==WifiPhase::Listening){if(net_.accept()){phase_=WifiPhase::Headers;header_bytes_=0;accepted_=progress_=now;}return;}
 if(std::uint32_t(now-progress_)>15000||std::uint32_t(now-accepted_)>120000){finishConnection();return;}
 if(phase_==WifiPhase::Headers){
  // Bytewise only for bounded headers; never consume upload bytes accidentally.
  for(unsigned i=0;i<256;++i){char c;const auto n=net_.receive(&c,1);if(n==-2)return;if(n<=0){finishConnection();return;}progress_=now;
   if(c==0||header_bytes_==sizeof(headers_)-1){error(431,"Request headers too large or invalid");return;}
   headers_[header_bytes_++]=c;headers_[header_bytes_]=0;
   if(header_bytes_>=4&&std::memcmp(headers_+header_bytes_-4,"\r\n\r\n",4)==0){request(input);return;}
  }return;
 }
 if(phase_==WifiPhase::Upload){
  if(remaining_){const auto n=net_.receive(input_,std::min(remaining_,sizeof(input_)));if(n==-2)return;if(n<=0){finishConnection();return;}remaining_-=std::size_t(n);progress_=now;
   const auto step=text_.feed({input_,std::size_t(n)},{output_,sizeof(output_)});if(!append(step))return;if(step.consumed!=std::size_t(n)||step.flow!=wx::Flow::NeedInput){error(500,"TXT streaming contract error");return;}
  }
  if(!remaining_){const auto step=text_.finish({output_,sizeof(output_)});if(append(step)&&step.flow==wx::Flow::Done)phase_=WifiPhase::Verify;}return;
 }
 if(phase_==WifiPhase::Verify){const auto s=sink_.finish(operation_,body_bytes_);progress_=now;if(s==wx::Status::Busy)return;if(s!=wx::Status::Ok){error(httpStatus(s),"Import verification failed; CURRENT unchanged");return;}phase_=WifiPhase::Publish;publishing_=true;return;}
 if(phase_==WifiPhase::Publish){const auto s=sink_.publish(operation_);progress_=now;if(s!=wx::Status::Busy)imported(s);return;}
 if(phase_==WifiPhase::Send){
  const bool header=reply_at_<reply_bytes_;const char* data=header?reply_:payload_;auto& at=header?reply_at_:payload_at_;const auto size=header?reply_bytes_:payload_bytes_;
  if(at==size){
   // An early UTF-8/body-limit rejection can leave accepted upload bytes in
   // TCP. Drain only the bounded Content-Length before closing, otherwise a
   // reset caused by unread input may hide the useful HTTP error in Safari.
   // The import gate is already released; idle/total timeouts still apply.
   if(remaining_){
    const auto n=net_.receive(input_,std::min(remaining_,sizeof(input_)));
    if(n==-2)return;
    if(n<=0){finishConnection();return;}
    remaining_-=std::size_t(n);progress_=now;return;
   }
   finishConnection();return;
  }
  const auto n=net_.send(data+at,std::min<std::size_t>(1024,size-at));if(n==-2)return;if(n<=0){finishConnection();return;}at+=std::size_t(n);progress_=now;
 }
}
}
