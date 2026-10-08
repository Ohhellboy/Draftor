"""Make fault-injection fixtures from actual C++ output; no parallel production codec."""
from pathlib import Path
import json
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from support import fixture, packets, rewrite, slip_packet
source=packets(fixture('H01'))
# Damaged/truncated bytes followed by valid frames; loss is repaired by a supplied snapshot.
corrupt=bytearray(source[1]); corrupt[20]^=0x40
sync=rewrite(source[-1],4,'<H',4)
(ROOT/'tests/fixtures/horizontal/H13.bin').write_bytes(
    slip_packet(source[0])+slip_packet(bytes(corrupt))+b'\xc1\x44\x54\xdb\xc0'+
    b''.join(slip_packet(p) for p in source[3:-1])+slip_packet(sync)+b'\xc1\x44\x54')
# A structurally valid future event occupies its real sequence number.
unknown=rewrite(source[1],4,'<H',60000)
(ROOT/'tests/fixtures/horizontal/H14.bin').write_bytes(
    slip_packet(source[0])+slip_packet(unknown)+b''.join(slip_packet(p) for p in source[2:-1])+slip_packet(sync))
names=['Short Chinese chain','Composition growth','Segmented selection continuation','Digit routed to candidate',
       'Candidate navigation across window','Composition backspace','Reopen/reselect with stale snapshot',
       'Document backspace/delete','Enter/newline','English direct insertion','File operation overlapping IME',
       'Queue saturation and sync','Corrupt/truncated frames and recovery','Unknown future event','BASIC privacy canary',
       'DETAILED content','OFF no events']
manifest={'evidence':'HOST / SIMULATED_SERIAL_CALLBACK; no product integration',
          'horizontal':{f'H{i:02d}':name for i,name in enumerate(names,1)},
          'vertical_compat':{'V01':'Editor logical navigation','V02':'Candidate navigation','V03':'Mode switch preserves business identities'}}
(ROOT/'tests/fixtures/manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Diagnostics are complete records outside behavior frame markers, not raw interleaving.
text_before=b'[BOOT] synthetic diagnostic\n'
text_between=('[RESP] '+('diagnostic long line '*200)+'\nline two\n').encode()
text_after='[DONE] 多行诊断\nlast line\n'.encode()
(ROOT/'tests/fixtures/preintegration/P06.bin').write_bytes(text_before+slip_packet(source[0])+text_between+b''.join(slip_packet(p) for p in source[1:])+text_after)
(ROOT/'tests/fixtures/preintegration/P07.bin').write_bytes(text_before+slip_packet(source[0])+text_between+slip_packet(bytes(corrupt))+b'[INFO] following corrupt frame\n'+b''.join(slip_packet(p) for p in source[2:-1])+slip_packet(sync)+text_after)
manifest['preintegration']={'P01':'InputScheme and Candidate/Literal context state/SYNC','P02':'Synthetic runtime identity',
 'P03':'Workload denominators by scheme','P04':'Causal view matching','P05':'Boundary and lifecycle/file enums',
 'P06':'Mixed diagnostics + behavior','P07':'Mixed diagnostics around genuine corruption','P08':'Largest health payload plus maximum detail bounds'}
(ROOT/'tests/fixtures/manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
