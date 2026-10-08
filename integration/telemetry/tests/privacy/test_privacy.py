import json
import unittest
from support import *

class PrivacyTests(unittest.TestCase):
    def test_basic_canaries_never_reach_wire_or_decoder(self):
        data=fixture('H15'); decoded=decode(data)
        for canary in (b'SECRET','秘密'.encode(),b'PATH_X',(0x534543524554).to_bytes(8,'little')):
            self.assertNotIn(canary,data)
        self.assertTrue(all('detail' not in event for event in decoded))
        self.assertFalse(any(e.get('event_type')=='CANDIDATE_ITEM_DETAIL' for e in decoded))
        self.assertTrue(any(e.get('event_type')=='DOCUMENT_MUTATION' for e in decoded))
        self.assertEqual([e['event_seq'] for e in decoded],list(range(1,len(decoded)+1)))

    def test_detailed_and_basic_same_semantics(self):
        detailed=events('H16'); basic=events('H15')
        structural=[e for e in detailed if e['event_type']!='CANDIDATE_ITEM_DETAIL']
        self.assertEqual([(e['event_type'],e['fields']) for e in basic],[(e['event_type'],e['fields']) for e in structural])
        self.assertTrue(any('秘密' in e['detail']['text'] for e in detailed))
        self.assertTrue(any(e['detail']['candidate_identity'] for e in detailed))
        self.assertTrue(any(e['event_type']=='INPUT_RECEIVED' and e['detail']['printable_codepoint']==ord('1') for e in detailed))

    def test_off_empty(self):
        self.assertEqual(fixture('H17'),b''); self.assertEqual(decode(fixture('H17')),[])
        self.assertEqual(summary([])['sessions'],{})
