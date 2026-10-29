 // R1_MENU_FOOTER_BEGIN: menu renderer owns its hints, including rename IME.
 if(s3app::display::menuOwnsFooter(draftor::board::Waveshare,menuPage)){
  s3app::display::menuPairingHint(ble.phase==keyboard::Phase::Pairing,ble.passkey,
   app->renameOpen()&&imeVisible?compositionY-12:328,menuFill,menuAux);
 }else if(app&&app->findOpen()){
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
