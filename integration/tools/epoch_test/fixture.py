#!/usr/bin/env python3
"""Extract the actual MCU admission/invalidate bodies for Host mutex adaptation."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
def fn(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
source='#pragma once\n#include "hid_keyboard.hpp"\n#include <atomic>\n#include <mutex>\nnamespace ble_epoch_fixture {\ninline std::mutex stateMux;\n#define portENTER_CRITICAL(p) (p)->lock()\n#define portEXIT_CRITICAL(p) (p)->unlock()\ninline std::atomic<uint32_t> epoch{1};\ninline std::atomic<bool> accepting{true};\ninline s3hid::Tracker tracker;\ninline bool requireRelease=false;\n'
source+='inline '+fn('void invalidate()')+'\ninline '+fn('bool admit(')
source+='\ninline void reset(){epoch=1;accepting=true;tracker.clear();requireRelease=false;}\n}\n#undef portENTER_CRITICAL\n#undef portEXIT_CRITICAL\n'
Path(sys.argv[2]).write_text(source)
