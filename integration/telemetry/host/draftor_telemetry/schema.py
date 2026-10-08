"""One machine-readable registry drives C++ generation and Host field validation."""
import json
from pathlib import Path
SCHEMA = json.loads((Path(__file__).resolve().parents[2] / 'schema/events.json').read_text())
EVENTS = {event['id']: event for event in SCHEMA['events']}
ENUMS = SCHEMA['enums']
WIRE = SCHEMA['wire']
SCHEMA_REVISION = WIRE['schema_revision']
WIDTHS = {'u8': 1, 'u32': 4, 'u64': 8}
HEADER_BYTES = WIRE['header_bytes']
MAX_PAYLOAD_BYTES = WIRE['max_payload_bytes']
MAX_PACKET_BYTES = HEADER_BYTES + MAX_PAYLOAD_BYTES + 2
START, END, ESC = 0xC1, 0xC0, 0xDB
ENVELOPE_NAMES = ('boot_id', 'event_seq', 'device_monotonic_us', 'action_id',
                  'composition_id', 'composition_revision', 'candidate_snapshot_id',
                  'document_revision', 'layout_revision')
STATE_EVENTS = {'SESSION_START', 'STATE_SNAPSHOT', 'STATE_CHANGE', 'SYNC_SNAPSHOT'}

def valid_enum(field, value):
    enum_name=field.get('enum')
    if enum_name is None:
        return True
    values=ENUMS[enum_name]
    disallowed={values[name] for name in field.get('disallowed_enum_values',[])}
    return value in values.values() and value not in disallowed
