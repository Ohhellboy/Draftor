#pragma once
#include <cstdio>
#include <cstdint>
namespace s3app::display {
inline bool menuOwnsFooter(bool waveshare,bool menuPage) noexcept {return waveshare&&menuPage;}
// Called only by the menu owner. Normal hints/errors remain in documentMenu /
// menuFeedback. Pairing has priority in one cleared line, never over old text.
// Rename candidates stay below renameBottom; the name/error area ends above it.
template<class Fill,class Aux>
void menuPairingHint(bool pairing,uint32_t passkey,int renameBottom,Fill fill,Aux aux){
 if(!pairing||passkey>=1000000)return;
 const int y=renameBottom<328?36:372;
 char pin[32];std::snprintf(pin,sizeof(pin),"Pair: %06lu + Enter",static_cast<unsigned long>(passkey));
 fill(0,y,300,24,false);aux(pin,8,y,false);
}
}
