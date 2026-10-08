#pragma once
#include <cstdint>
#include <string_view>

namespace s3app {
// This is a CN body-only interpretation of raw keyboard usages, not a second
// keyboard layout. An unlisted key continues through the existing UK path.
struct CnPunctuationKey {
 char symbol=0; // US punctuation semantic, independent of UK ASCII decoding.
 std::string_view explicit_side{}; // Secondary R1 quote aliases only.
};
inline CnPunctuationKey resolveCnPunctuationKey(uint8_t usage,uint8_t mods) noexcept {
 const bool shift=(mods&0x22)!=0;
 const auto other=mods&0xdd;
 if(other==0x04){ // Exact left Alt, with either Shift; never AltGr/Ctrl/GUI.
  struct Alias {uint8_t usage;std::string_view plain,shifted;};
  static constexpr Alias aliases[]={
   {47,"“","‘"},{48,"”","’"},{54,{},"『"},{55,{},"』"}
  };
  for(const auto& key:aliases)if(key.usage==usage)return {0,shift?key.shifted:key.plain};
  return {};
 }
 if(other)return {};
 struct Position {uint8_t usage;char plain,shifted;};
 static constexpr Position positions[]={
  {54,',','<'},{55,'.','>'},{51,';',':'},{56,0,'?'},
  {47,'[','{'},{48,']','}'},{52,'\'','"'},
  // The existing UK table accepts both Enter-area usages as #/~. Cover both;
  // ISO's extra backslash position remains an explicitly supported alias.
  {49,'\\',0},{50,'\\',0},{100,'\\',0},
  {30,0,'!'},{35,0,'^'},{38,0,'('},{39,0,')'},{45,0,'_'}
 };
 for(const auto& key:positions)if(key.usage==usage)return {shift?key.shifted:key.plain,{}};
 return {};
}

// Count both sides before the actual caret. No toggle, cache, allocation or
// persistent metadata: deletion, undo and reopening naturally change parity.
inline bool oddQuotePrefix(std::string_view body,size_t caret,bool double_quote) noexcept {
 const auto end=caret<body.size()?caret:body.size();
 const unsigned char left=double_quote?0x9c:0x98;
 bool odd=false;
 for(size_t i=0;i+2<end;++i){
  if(static_cast<unsigned char>(body[i])==0xe2&&static_cast<unsigned char>(body[i+1])==0x80){
   const auto tail=static_cast<unsigned char>(body[i+2]);
   if(tail==left||tail==left+1){odd=!odd;i+=2;}
  }
 }
 return odd;
}

// Call only after IME, modal fields and product commands have assigned BODY.
inline std::string_view cnBodyPunctuation(uint8_t usage,uint8_t mods,
                                         std::string_view body,size_t caret) noexcept {
 const auto key=resolveCnPunctuationKey(usage,mods);
 if(!key.explicit_side.empty())return key.explicit_side;
 switch(key.symbol){
  case ',':return "，";case '.':return "。";
  case ';':return "；";case ':':return "：";
  case '!':return "！";case '?':return "？";
  case '(':return "（";case ')':return "）";
  case '\\':return "、";case '[':return "【";case ']':return "】";
  case '<':return "《";case '>':return "》";
  case '{':return "「";case '}':return "」";
  case '^':return "……";case '_':return "——";
  case '"':return oddQuotePrefix(body,caret,true)?"”":"“";
  case '\'':return oddQuotePrefix(body,caret,false)?"’":"‘";
  default:return {};
 }
}
} // namespace s3app
