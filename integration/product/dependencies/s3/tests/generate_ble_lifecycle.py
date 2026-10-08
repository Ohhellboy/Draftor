#!/usr/bin/env python3
"""Compile production connect/cleanup functions and selected GAP branches against deterministic stubs.
No copied implementation in the tests. The omitted GAP branches are unrelated HID/security payload handling.
This is a control-flow test, not a NimBLE protocol-stack simulation or device test.
"""
from pathlib import Path
import sys,re
p=Path(__file__).resolve().parents[1]
s=(p/'libraries/BLE/src/BLEClient.cpp').read_text();n=s.index('NimBLE functions')
def function(signature):
 a=s.index(signature,n);b=s.index('\n/**',a);return s[a:b]
connect=function('bool BLEClient::connect(BLEAddress address, uint8_t type, uint32_t timeoutMs)')
a=s.index('int BLEClient::handleGAPEvent(struct',n);b=s.index('\n/**',a);gap=s[a:b]
first=gap.index('    case BLE_GAP_EVENT_DISCONNECT:');tail=gap.index('  }  // Switch')
positions=[m.start() for m in re.finditer(r'^    case BLE_GAP_EVENT_|^    default:',gap[first:tail],flags=re.M)]+[tail-first]
parts=[gap[first+x:first+y] for x,y in zip(positions,positions[1:])]
wanted=['DISCONNECT','CONNECT','TERM_FAILURE','MTU']
body=''.join(t for t in parts if any(t.startswith('    case BLE_GAP_EVENT_'+k+':') for k in wanted))
assert len([t for t in parts if any(t.startswith('    case BLE_GAP_EVENT_'+k+':') for k in wanted)])==4
gap=gap[:first]+body+'    default: return 0;\n'+gap[tail:]
a=s.index('int BLEClient::disconnect(uint8_t reason)',n);b=s.index('\n/**',a);retire=s[a:b]
a=(p/'ble_keyboard_host.cpp').read_text();start=a.index('enum class ReleaseStage');end=a.index('void lifecycle(',start);context=a[start:end]
start=a.index('bool releaseClient(');end=a.index('\nvoid task(',start);release=a[start:end]
out=Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
out.write_text('#include "ble_lifecycle_stubs.hpp"\n'+connect+gap+retire+context+release+'\n#include "ble_lifecycle_cases.hpp"\n')
print('Generated actual production connect/disconnect/retireStep, four GAP cases, releaseClient and releaseCallback')
