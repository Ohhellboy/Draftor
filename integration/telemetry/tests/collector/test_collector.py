import json
import os
from pathlib import Path
import pty
import signal
import subprocess
import tempfile
import time
import unittest
from support import *
from draftor_telemetry.collector import ArchiveCollector, replay, reconstruct_archive

class CollectorTests(unittest.TestCase):
    def test_rotation_exact_bytes_and_manifest_replay_timestamps(self):
        data=fixture('H16')
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            first=Path(directory)/'first'; collector=ArchiveCollector(first,127)
            for offset in range(0,len(data),19): collector.feed(data[offset:offset+19],STAMP)
            collector.close('input_eof'); result=reconstruct_archive(first)
            manifest=json.loads((first/'manifest.json').read_text())
            self.assertEqual(manifest['bytes'],len(data)); self.assertEqual(manifest['stop_reason'],'input_eof')
            self.assertTrue(all(p['bytes']<=127 for p in manifest['files']))
            self.assertEqual(b''.join((first/p['file']).read_bytes() for p in manifest['files']),data)
            self.assertTrue(result['sessions'])
            second=Path(directory)/'second'; collector=ArchiveCollector(second,93)
            replay(first/'manifest.json',collector,11); collector.close('input_eof')
            self.assertEqual((first/'decoded.ndjson').read_text(),(second/'decoded.ndjson').read_text())

    def test_transport_boundary_is_replayed(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            first=Path(directory)/'first'; collector=ArchiveCollector(first,99)
            collector.feed(fixture('H01')[:20],STAMP)
            collector.transport_gap('serial_disconnect',STAMP)
            collector.feed(fixture('H01'),STAMP); collector.close('ctrl_c')
            second=Path(directory)/'second'; collector=ArchiveCollector(second,100)
            replay(first/'manifest.json',collector); collector.close('input_eof')
            self.assertEqual((first/'decoded.ndjson').read_text(),(second/'decoded.ndjson').read_text())
            records=[json.loads(line) for line in (first/'decoded.ndjson').read_text().splitlines()]
            self.assertEqual(records[0]['error'],'truncated_final_frame')
            self.assertEqual(records[1]['record_type'],'transport_gap')

    def test_truncated_tail_keeps_previous_records(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            output=Path(directory)/'capture'; collector=ArchiveCollector(output,4096)
            collector.feed(fixture('H01')+b'\xc1DT',STAMP); collector.close('ctrl_c')
            records=[json.loads(line) for line in (output/'decoded.ndjson').read_text().splitlines()]
            self.assertEqual(records[-1]['error'],'truncated_final_frame')
            self.assertGreater(len(records),20)
            self.assertEqual(next(iter(reconstruct_archive(output)['sessions'].values()))['status'],'NEEDS_RESYNC')

    def test_existing_output_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            Path(directory,'user-file').write_text('do not overwrite')
            with self.assertRaises(ValueError): ArchiveCollector(directory)
            self.assertEqual(Path(directory,'user-file').read_text(),'do not overwrite')

    @unittest.skipUnless(os.name=='posix','PTY requires POSIX')
    def test_posix_serial_partial_reads_and_graceful_ctrl_c(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            master,slave=pty.openpty()
            output=Path(directory)/'serial'
            process=subprocess.Popen([sys.executable,str(ROOT/'host/telemetry.py'),'serial','--port',os.ttyname(slave),
                                      '--output',str(output),'--rotate-bytes','127'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+5
                while not (output/'manifest.json').exists() and time.monotonic()<deadline: time.sleep(.02)
                time.sleep(.15)
                data=fixture('H16')
                for offset in range(0,len(data),23): os.write(master,data[offset:offset+23]); time.sleep(.001)
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    manifest=json.loads((output/'manifest.json').read_text())
                    if manifest['bytes']==len(data): break
                    time.sleep(.02)
                self.assertIsNone(process.poll(),'serial must continue until Ctrl-C')
                process.send_signal(signal.SIGINT)
                stdout,stderr=process.communicate(timeout=5)
                self.assertEqual(process.returncode,0,(stdout,stderr))
                manifest=json.loads((output/'manifest.json').read_text())
                self.assertEqual(manifest['stop_reason'],'ctrl_c'); self.assertEqual(manifest['bytes'],len(data))
                self.assertEqual(manifest['serial_settings']['baud'],115200)
                self.assertEqual(b''.join((output/p['file']).read_bytes() for p in manifest['files']),data)
                self.assertTrue(next(iter(reconstruct_archive(output)['sessions'].values()))['history_complete'])
            finally:
                if process.poll() is None: process.kill(); process.communicate()
                os.close(master); os.close(slave)

    @unittest.skipUnless(os.name=='posix','PTY requires POSIX')
    def test_serial_disconnect_explicit_stop(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            master,slave=pty.openpty(); output=Path(directory)/'serial'
            process=subprocess.Popen([sys.executable,str(ROOT/'host/telemetry.py'),'serial','--port',os.ttyname(slave),
                                      '--output',str(output),'--no-reconnect'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+5
                while not (output/'manifest.json').exists() and time.monotonic()<deadline: time.sleep(.02)
                time.sleep(.2); os.close(master); master=None
                stdout,stderr=process.communicate(timeout=5)
                self.assertEqual(process.returncode,0,(stdout,stderr))
                manifest=json.loads((output/'manifest.json').read_text())
                self.assertEqual(manifest['stop_reason'],'serial_disconnected')
                self.assertIn('transport_gap',(output/'decoded.ndjson').read_text())
            finally:
                if process.poll() is None: process.kill(); process.communicate()
                if master is not None: os.close(master)
                os.close(slave)

    def test_serial_reconnect_contract(self):
        from unittest.mock import patch
        from draftor_telemetry.collector import collect_serial
        class Capture:
            def __init__(self): self.gaps=[]; self.chunks=[]
            def transport_gap(self,reason): self.gaps.append(reason)
            def feed(self,data): self.chunks.append(data)
        capture=Capture()
        # First open fails; the second succeeds and yields data; user interrupts.
        with patch('draftor_telemetry.collector.open_serial',side_effect=[OSError('missing'),123]) as opened, \
             patch('draftor_telemetry.collector.select.select',side_effect=[([123],[],[]),KeyboardInterrupt]), \
             patch('draftor_telemetry.collector.os.read',return_value=b'bytes'), \
             patch('draftor_telemetry.collector.os.close') as closed, \
             patch('draftor_telemetry.collector.time.sleep'):
            with self.assertRaises(KeyboardInterrupt): collect_serial('simulated',115200,capture)
        self.assertEqual(opened.call_count,2); self.assertEqual(len(capture.gaps),1)
        self.assertEqual(capture.chunks,[b'bytes']); closed.assert_called_once_with(123)

    def test_archive_write_error_is_not_reconnect(self):
        from unittest.mock import patch
        from draftor_telemetry.collector import collect_serial
        class FailingArchive:
            def feed(self,data): raise OSError('disk full')
            def transport_gap(self,reason): raise AssertionError('must not relabel disk error')
        with patch('draftor_telemetry.collector.open_serial',return_value=123), \
             patch('draftor_telemetry.collector.select.select',return_value=([123],[],[])), \
             patch('draftor_telemetry.collector.os.read',return_value=b'bytes'), \
             patch('draftor_telemetry.collector.os.close'):
            with self.assertRaisesRegex(OSError,'disk full'): collect_serial('simulated',115200,FailingArchive())

    def test_sigint_during_chunk_finishes_archive_transaction(self):
        from unittest.mock import patch
        from draftor_telemetry.collector import main
        original_feed=ArchiveCollector.feed
        triggered=[]
        def interrupted_feed(collector,data,received_time=None):
            if not triggered:
                triggered.append(True)
                signal.raise_signal(signal.SIGINT)
            original_feed(collector,data,received_time)
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            output=Path(directory)/'capture'
            with patch.object(ArchiveCollector,'feed',interrupted_feed):
                self.assertEqual(main(['replay','--input',str(ROOT/'tests/fixtures/horizontal/H16.bin'),
                                       '--output',str(output),'--rotate-bytes','127']),0)
            manifest=json.loads((output/'manifest.json').read_text())
            self.assertEqual(manifest['stop_reason'],'ctrl_c')
            self.assertEqual(manifest['bytes'],len(fixture('H16')))
            replayed=ArchiveCollector(Path(directory)/'replay')
            replay(output/'manifest.json',replayed)
            replayed.close('input_eof')
            self.assertEqual((output/'decoded.ndjson').read_text(),(Path(directory)/'replay/decoded.ndjson').read_text())
