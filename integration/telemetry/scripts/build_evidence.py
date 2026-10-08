"""Retain compact review evidence, not repeated build output or temporary executables."""
import binascii
import json
from pathlib import Path
import shutil
import sys
import timeit
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'host'),str(ROOT/'tests')]
from support import decode, fixture, packets, session, summary, StreamDecoder, STAMP
from draftor_telemetry.collector import ArchiveCollector, replay, reconstruct_archive
index={}
for path in sorted((ROOT/'tests/fixtures').rglob('*.bin')):
    mixed=path.stem in ('P06','P07')
    decoder=StreamDecoder(mixed=mixed)
    records=decoder.feed(path.read_bytes(),STAMP)+decoder.finish(STAMP)
    reconstructed=summary(records)
    index[path.stem]={'raw_bytes':path.stat().st_size,'records':len(records),'stream_mode':'mixed' if mixed else 'behavior',
                      'diagnostic_chunks':sum(r['record_type']=='diagnostic' for r in records),
                      'decode_errors':[e['error'] for e in records if e['record_type']=='decode_error'],
                      'sessions':{key:{'status':s['status'],'history_complete':s['history_complete'],
                                       'gaps':s['gaps'],'actions':len(s['actions']),
                                       'compositions':len(s['compositions']),'metrics':s['metrics']}
                                  for key,s in reconstructed['sessions'].items()}}
(ROOT/'evidence/fixture_results.json').write_text(json.dumps(index,indent=2)+'\n')
# This one archive is retained to make the full output format inspectable.
archive=ROOT/'evidence/horizontal_detailed_capture'
if archive.exists(): shutil.rmtree(archive)
collector=ArchiveCollector(archive,1024,source_kind='HOST_SYNTHETIC_FILE_REPLAY')
replay(ROOT/'tests/fixtures/horizontal/H16.bin',collector,43)
collector.close('input_eof'); reconstruct_archive(archive)
mixed_archive=ROOT/'evidence/mixed_capture'
if mixed_archive.exists(): shutil.rmtree(mixed_archive)
collector=ArchiveCollector(mixed_archive,1024,source_kind='HOST_SYNTHETIC_MIXED_FILE_REPLAY',stream_mode='mixed')
replay(ROOT/'tests/fixtures/preintegration/P07.bin',collector,43)
collector.close('input_eof'); reconstruct_archive(mixed_archive)
packet=packets(fixture('H16'))[0]
iterations=100000
elapsed=timeit.timeit(lambda:binascii.crc_hqx(packet[:-2],0xffff),number=iterations)
(ROOT/'evidence/host_crc_benchmark.json').write_text(json.dumps({'evidence':'HOST Python binascii; not S3',
    'iterations':iterations,'packet_without_crc_bytes':len(packet)-2,'mean_ns':elapsed*1e9/iterations},indent=2)+'\n')
print('PASS fixture evidence, complete output archive, HOST CRC benchmark')
