import base64
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from support import *
from draftor_telemetry.collector import ArchiveCollector, replay, reconstruct_archive, main
from draftor_telemetry.decoder import decode_packet
from draftor_telemetry.schema import ENUMS, EVENTS, SCHEMA_REVISION, WIRE, WIDTHS


def mixed_records(data,size=13):
    decoder=StreamDecoder(mixed=True); records=[]
    for offset in range(0,len(data),size): records+=decoder.feed(data[offset:offset+size],STAMP)
    return records+decoder.finish(STAMP)


def diagnostic_bytes(records):
    return b''.join(base64.b64decode(r['bytes_base64']) for r in records if r['record_type']=='diagnostic')


class PreintegrationTests(unittest.TestCase):
    def test_mixed_before_between_after_long_multiline_fragmented(self):
        data=fixture('P06')
        expected=None
        for size in (1,2,7,13,512,100000):
            records=mixed_records(data,size)
            self.assertFalse(any(r['record_type']=='decode_error' for r in records))
            self.assertTrue(all(r['bytes']<=WIRE['diagnostic_chunk_bytes'] for r in records if r['record_type']=='diagnostic'))
            behavior=[r for r in records if r['record_type']=='event']
            self.assertEqual(behavior,events('H01'))
            diagnostics=diagnostic_bytes(records)
            if expected is None: expected=diagnostics
            self.assertEqual(diagnostics,expected)
            self.assertTrue(diagnostics.startswith(b'[BOOT]'))
            self.assertIn('多行诊断'.encode(),diagnostics)
            self.assertTrue(diagnostics.endswith(b'last line\n'))
            s=next(iter(summary(records)['sessions'].values()))
            self.assertEqual(s['gaps'],[]); self.assertEqual(s['status'],'RELIABLE')

    def test_crc_corruption_surrounded_by_text_still_creates_gap(self):
        records=mixed_records(fixture('P07'))
        self.assertEqual([r['error'] for r in records if r['record_type']=='decode_error'],['crc_mismatch'])
        self.assertIn(b'following corrupt frame',diagnostic_bytes(records))
        s=next(iter(summary(records)['sessions'].values()))
        self.assertFalse(s['history_complete']); self.assertEqual(s['status'],'RELIABLE')
        self.assertTrue(any(g['kind']=='crc_mismatch' for g in s['gaps']))

    def test_printable_binary_failure_never_reclassified_as_text(self):
        records=mixed_records(b'[OK]\n\xc1all printable but a bad behavior frame\xc0[END]\n')
        self.assertEqual(diagnostic_bytes(records),b'[OK]\n[END]\n')
        self.assertEqual(sum(r['record_type']=='decode_error' for r in records),1)

    def test_binary_truncation_and_invalid_escape_preserved(self):
        first=slip_packet(packets(fixture('H01'))[0])
        for damaged,error in [(b'\xc1DT\xdb\x41\xc0','invalid_escape'),(b'\xc1DT','truncated_frame')]:
            records=mixed_records(b'[BOOT]\n'+damaged+first+b'[END]\n')
            self.assertIn(error,[r['error'] for r in records if r['record_type']=='decode_error'])
            self.assertEqual([r['event_type'] for r in records if r['record_type']=='event'],['SESSION_START'])
        records=mixed_records(first+b'[text]\n\xc1DT')
        self.assertEqual(records[-1]['error'],'truncated_final_frame')

    def test_mixed_archive_preserves_raw_diagnostics_and_manifest_replay(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            output=Path(directory)/'first'
            collector=ArchiveCollector(output,83,stream_mode='mixed')
            for offset in range(0,len(fixture('P06')),37): collector.feed(fixture('P06')[offset:offset+37],STAMP)
            collector.close('input_eof'); reconstructed=reconstruct_archive(output)
            manifest=json.loads((output/'manifest.json').read_text())
            self.assertEqual(manifest['schema_revision'],SCHEMA_REVISION)
            self.assertEqual(manifest['stream_mode'],'mixed')
            self.assertEqual(b''.join((output/p['file']).read_bytes() for p in manifest['files']),fixture('P06'))
            self.assertTrue((output/'diagnostics.ndjson').stat().st_size)
            self.assertNotIn('diagnostic',(output/'decoded.ndjson').read_text())
            self.assertTrue(next(iter(reconstructed['sessions'].values()))['history_complete'])
            replayed=Path(directory)/'second'
            self.assertEqual(main(['replay','--input',str(output/'manifest.json'),'--output',str(replayed)]),0)
            self.assertEqual((output/'decoded.ndjson').read_bytes(),(replayed/'decoded.ndjson').read_bytes())
            self.assertEqual((output/'diagnostics.ndjson').read_bytes(),(replayed/'diagnostics.ndjson').read_bytes())

    def test_input_scheme_and_candidate_literal_state_sync(self):
        records=events('P01')
        self.assertEqual(records[0]['fields']['input_scheme'],ENUMS['InputScheme']['FullPinyin'])
        snapshots=[e['fields'] for e in records if e['event_type']=='SYNC_SNAPSHOT']
        self.assertEqual([(s['input_scheme'],s['edit_context']) for s in snapshots],[(2,4),(3,5)])
        s=session('P01')
        self.assertEqual(s['state']['input_scheme'],3); self.assertEqual(s['state']['edit_context'],5)

    def test_runtime_identity_encode_decode_replay(self):
        identity=next(e for e in events('P02') if e['event_type']=='RUNTIME_IDENTITY')
        self.assertEqual(identity['fields']['identity_scope'],ENUMS['IdentityScope']['Synthetic'])
        self.assertEqual(identity['fields']['identity_namespace'],0xffff0001)
        for field in ('firmware_build_id','core_api_family_id','input_host_family_id','resource_generation','rule_policy_generation','runtime_config_id'):
            self.assertGreater(identity['fields'][field],0)
        self.assertNotIn('detail',identity)
        self.assertEqual(set(('p2','p3','prefix_slots','tf_frequency_hz','profile_id','locate','prefetch','learning_mode'))-identity['fields'].keys(),set())
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            collector=ArchiveCollector(Path(directory)/'capture')
            replay(ROOT/'tests/fixtures/preintegration/P02.bin',collector); collector.close('input_eof')
            s=next(iter(reconstruct_archive(Path(directory)/'capture')['sessions'].values()))
            self.assertEqual(s['runtime_identities'][0]['fields'],identity['fields'])
            self.assertTrue(all(e['runtime_identity_seq']==identity['event_seq'] for e in s['events'][2:]))

    def test_boundary_lifecycle_and_file_values_preserved(self):
        records=events('P05')
        navigation=next(e for e in records if e['event_type']=='CANDIDATE_NAVIGATION')
        self.assertEqual(navigation['fields']['outcome'],ENUMS['NavigationOutcome']['BoundaryHit'])
        self.assertEqual(navigation['fields']['previous_index'],navigation['fields']['global_index'])
        self.assertEqual({e['fields']['outcome'] for e in records if e['event_type']=='COMPOSITION_END'},{3,4})
        self.assertEqual({e['fields']['operation'] for e in records if e['event_type']=='FILE_OP_END'},set(range(1,8)))

    def test_output_count_explicit_unit_and_unknown(self):
        accepted=next(e for e in events('H01') if e['event_type']=='SELECTION_RESULT')
        self.assertEqual(accepted['fields']['confirmed_output_count'],2)
        self.assertEqual(accepted['fields']['output_count_unit'],ENUMS['OutputCountUnit']['UnicodeScalar'])
        rejected=next(e for e in events('H07') if e['event_type']=='SELECTION_RESULT' and e['fields']['outcome']==2)
        self.assertEqual(rejected['fields']['confirmed_output_count'],0xffffffff)
        self.assertEqual(rejected['fields']['output_count_unit'],ENUMS['OutputCountUnit']['Unknown'])

    def test_view_reason_action_and_layout_causality(self):
        s=session('P04'); samples=s['metrics']['latencies']['candidate_to_view']['samples']
        self.assertEqual(len(samples),1)
        views=[e for e in s['events'] if e['event_type']=='VIEW_READY']
        self.assertEqual(samples[0]['end_seq'],views[-1]['event_seq'])
        self.assertNotEqual(samples[0]['end_seq'],views[0]['event_seq'])

    def test_workload_denominators_and_scheme_groups(self):
        metrics=session('P03')['metrics']
        self.assertNotIn('composition_raw_length_distribution',metrics)
        self.assertEqual(metrics['composition_peak_raw_length_distribution'],{'5':1,'4':1})
        self.assertEqual(metrics['composition_terminal_raw_length_distribution'],{'5':1,'3':1})
        self.assertEqual(metrics['selection_consumed_raw_length_distribution'],{'5':1,'3':1})
        self.assertEqual(metrics['revision_raw_length_distribution'],{'2':3,'5':2,'4':1,'3':2,'20':1})
        self.assertEqual(metrics['workload_denominators']['revision_raw_length_distribution']['sample_count'],9)
        self.assertEqual(metrics['workload_denominators']['composition_peak_raw_length_distribution']['sample_count'],2)
        self.assertEqual(metrics['workload_by_input_scheme']['FullPinyin']['composition_peak_raw_length_distribution'],{'5':1})
        self.assertEqual(metrics['workload_by_input_scheme']['XiaoheShuangpin']['composition_terminal_raw_length_distribution'],{'3':1})
        self.assertEqual(metrics['composition_workload_exclusions'],{'incomplete_or_ambiguous_lifecycle':1})

    def test_incomplete_gap_composition_not_reported_as_full_workload(self):
        records=events('H01')
        records.insert(7,{'record_type':'transport_gap','error':'test loss','host_received_time':STAMP})
        s=next(iter(summary(records)['sessions'].values()))
        self.assertEqual(s['metrics']['composition_peak_raw_length_distribution'],{})
        self.assertTrue(s['metrics']['composition_workload_exclusions'])

    def test_enum_registry_drives_decoder_without_hardcoded_sets(self):
        packet=packets(fixture('P01'))[0]
        edited=rewrite(packet,58,'<B',99)
        self.assertEqual(decode_packet(edited,STAMP)['error'],'invalid_enum')
        with patch.dict(ENUMS['InputScheme'],{'TestNewScheme':99}):
            self.assertEqual(decode_packet(edited,STAMP)['fields']['input_scheme'],99)
        edited=rewrite(packet,59,'<B',99)
        with patch.dict(ENUMS['EditContext'],{'TestNewContext':99}):
            self.assertEqual(decode_packet(edited,STAMP)['fields']['edit_context'],99)

    def test_every_declared_enum_field_rejects_unknown(self):
        seen=set()
        for name in ('H12','H16','H11','H08','V03','H05','H07','P01','P02','P05'):
            for packet in packets(fixture(name)):
                decoded=decode_packet(packet,STAMP)
                if decoded['record_type']!='event': continue
                spec=EVENTS[decoded['event_id']]; offset=56
                for field in spec['fields']:
                    if 'enum' in field:
                        result=decode_packet(rewrite(packet,offset,'<B',255),STAMP)
                        self.assertEqual(result['error'],'invalid_enum',(spec['name'],field))
                        self.assertEqual(result['field'],field['name']); seen.add(field['enum'])
                    offset+=WIDTHS[field['type']]
        self.assertEqual(seen,set(ENUMS))

    def test_legacy_revision_is_explicitly_rejected(self):
        packet=packets(fixture('H01'))[0]
        self.assertEqual(decode_packet(rewrite(packet,3,'<B',1),STAMP)['error'],'unsupported_schema_revision')

    def test_generated_schema_check_detects_drift(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            target=Path(directory)
            for name in ('scripts/generate_schema.py','scripts/schema_documentation.py','schema/events.json'):
                path=target/name; path.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(ROOT/name,path)
            (target/'include/draftor/telemetry').mkdir(parents=True); (target/'src').mkdir()
            command=[sys.executable,str(target/'scripts/generate_schema.py')]
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
            self.assertEqual(subprocess.run(command+['--check'],capture_output=True).returncode,0)
            with (target/'include/draftor/telemetry/EventSchema.h').open('a') as stream: stream.write('// drift\n')
            result=subprocess.run(command+['--check'],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0); self.assertIn('schema drift',result.stderr)

    def test_largest_health_payload_and_scratch_bound(self):
        records=events('P08')
        health=next(e for e in records if e['event_type']=='TELEMETRY_HEALTH')
        self.assertEqual(len(health['detail']['text']),WIRE['text_capacity'])
        self.assertEqual(len(health['detail']['path']),WIRE['path_capacity'])
        packet=packets(fixture('P08'))[-1]
        self.assertEqual(int.from_bytes(packet[6:8],'little'),258)
        self.assertLessEqual(len(packet),56+WIRE['max_payload_bytes']+2)
        for name in ('sink_busy','dropped_sink_failed','dropped_encode','transport_failures','queue_high_water','frames_accepted','bytes_accepted'):
            self.assertIn(name,health['fields'])
        self.assertNotIn('dropped_sink',health['fields'])

    def test_unrelated_document_refresh_cannot_finish_document_latency(self):
        records=events('H01')
        for event in records:
            if event['event_type']=='VIEW_READY' and event['action_id']==2:
                event['fields']['reason']=ENUMS['ViewReason']['ViewportMove']
        metrics=next(iter(summary(records)['sessions'].values()))['metrics']['latencies']
        self.assertEqual(metrics['document_to_view']['count'],0)
        self.assertEqual(metrics['document_to_display_ack']['count'],0)
        self.assertEqual(metrics['view_to_display_ack']['count'],2)
