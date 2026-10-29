s3app::TransferInputState usbInput(){return {ime&&ime->active(),ime&&ime->hasWork(),false};}
bool usbKey(const s3hid::Event& e){
 const bool traced=e.down&&e.usage==66;
 if(traced)rd::mark(rd::Stage::UsbKey);
 if(!usbRuntime){
  if(!s3app::usbEntryKey(*app,e))return false;
  rd::mark(rd::Stage::IntegrityBegin);
  if(!capture_test::capture(s3app::stability::Kind::UsbBefore,e.usage,e.epoch,0,true)){heapIntegrityStopped=true;rd::mark(rd::Stage::IntegrityFail);return true;}
  rd::mark(rd::Stage::IntegrityPass);rd::mark(rd::Stage::AllocateBegin);
  if(!s3app::ensureUsbRuntime(usbRuntime)){rd::mark(rd::Stage::AllocateDenied);capture_test::capture(s3app::stability::Kind::UsbDenied,sizeof(UsbRuntime));app->notice="USB unavailable: memory low";app->dirtyDisplay=true;return true;}
  rd::mark(rd::Stage::AllocateReady);
  capture_test::capture(s3app::stability::Kind::UsbReady,sizeof(UsbRuntime));
 }
 const auto before=usbRuntime->transfer.phase();
 if(traced)rd::mark(rd::Stage::DispatchBegin);
 const bool consumed=usbRuntime->transfer.key(e,usbInput());
 if(traced)rd::mark(rd::Stage::DispatchEnd,unsigned(usbRuntime->transfer.phase())|(unsigned(consumed)<<16));
 if(before!=usbRuntime->transfer.phase())capture_test::capture(s3app::stability::Kind::UsbPhase,unsigned(before),unsigned(usbRuntime->transfer.phase()));
 return consumed;
}
