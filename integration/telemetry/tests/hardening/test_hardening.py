import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from support import ROOT, decode, summary

class HardeningTests(unittest.TestCase):
    def test_invalid_and_suppressed_attempts_leave_no_host_sequence_gap(self):
        records=decode((ROOT/'evidence/hardening_stream.bin').read_bytes())
        self.assertTrue(all(r['record_type']=='event' for r in records))
        self.assertEqual([r['event_seq'] for r in records],[1,2,3,4])
        self.assertEqual([r['event_type'] for r in records],['SESSION_START','INPUT_RECEIVED','INPUT_RECEIVED','TELEMETRY_HEALTH'])
        health=records[-1]['fields']
        self.assertEqual(health['attempted'],5)
        self.assertEqual(health['enqueued'],3)
        self.assertEqual(health['dropped_invalid'],2)
        self.assertEqual(health['dropped_queue'],0)
        session=next(iter(summary(records)['sessions'].values()))
        self.assertEqual(session['gaps'],[])
        self.assertEqual(session['status'],'RELIABLE')
        self.assertTrue(session['history_complete'])

    def test_host_benchmark_has_neutral_label_and_real_environment(self):
        resources=json.loads((ROOT/'evidence/resources.json').read_text())
        self.assertEqual(resources['evidence'],'HOST synthetic; not S3')
        self.assertTrue(resources['host_environment']['machine'])
        self.assertTrue(resources['host_environment']['platform'])
        source=(ROOT/'examples/resource_benchmark.cpp').read_text()
        self.assertIn('HOST synthetic; not S3',source)
        self.assertNotIn('arm64',source)

    def test_required_metadata_drift_changes_core_validation(self): # C09
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as directory:
            target=Path(directory)
            for name in ('scripts/generate_schema.py','scripts/schema_documentation.py','schema/events.json'):
                path=target/name; path.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(ROOT/name,path)
            (target/'include/draftor/telemetry').mkdir(parents=True); (target/'src').mkdir()
            command=[sys.executable,str(target/'scripts/generate_schema.py')]
            subprocess.run(command,check=True,capture_output=True)
            subprocess.run(command+['--check'],check=True,capture_output=True)
            schema_path=target/'schema/events.json'
            schema=json.loads(schema_path.read_text())
            event=next(e for e in schema['events'] if e['name']=='INPUT_RECEIVED')
            event['required_envelope'].append({'field':'candidate_snapshot_id','nonzero':True})
            schema_path.write_text(json.dumps(schema))
            result=subprocess.run(command+['--check'],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('ValidateEnvelope.inc',result.stderr)
            subprocess.run(command,check=True,capture_output=True)
            subprocess.run(command+['--check'],check=True,capture_output=True)
            case=(target/'src/ValidateEnvelope.inc').read_text().split('case EventType::InputReceived:')[1].split('break;')[0]
            self.assertIn('source.envelope.candidate_snapshot_id == 0',case)
