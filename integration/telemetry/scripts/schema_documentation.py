"""Readable event reference generated from the authoritative schema metadata."""

def describe_required(rule):
    text = '`'+rule['field']+'` ('+('nonzero' if rule['nonzero'] else 'zero allowed')+')'
    if 'when' in rule:
        when = rule['when']
        condition = ' != 0' if when.get('nonzero') else ' = '+str(when['equals'])
        text += ' when `'+when['source']+'.'+when['field']+condition+'`'
    return text

def render_event_schema(schema):
    lines=[f'# Event Schema v1 / revision {schema["schema_revision"]} — Phase 2 Step 0', '',
           'Generated from `schema/events.json` by `scripts/generate_schema.py`. Numeric IDs are fixed; never reuse them.', '',
           'Pre-integration revision 2 updates the unreleased v1 schema; earlier Phase 1 binary captures are incompatible and are not silently interpreted. Fixtures are regenerated. See WIRE_FORMAT for the explicit revision tag and distinct frame markers.', '',
           '## Common envelope and units', '',
           'All events have boot_id:u64, event_seq:u64, device_monotonic_us:u64, action_id:u32, composition_id:u32, composition_revision:u32, candidate_snapshot_id:u32, document_revision:u32 and layout_revision:u32.', '',
           'boot_id is nonzero and unique across captured boots; event_seq starts at 1 and is assigned by the emitter. Device time is an integration-provided monotonic microsecond counter. Host UTC receive time is separate archive metadata.', '',
           'Business IDs are opaque counters, never reversible content identities. action_id identifies accepted input; composition_id persists across actions. work_id is unique per boot; selection_id, display_id and file_operation_id are each unique per boot. candidate_snapshot_id identifies a materialized structural snapshot, not a word. Revisions belong to their composition/document/layout owner and do not wrap within a boot.', '',
           'Required-envelope rules below are generated into Portable Core validation. Nonzero identity/revision rules apply only when declared; document/layout revisions may start at zero. State composition_id=0 declares no active composition; a nonzero composition_id requires its revision, without inferring activity from context/raw length. Candidate validity uses the existing scalar candidate_valid flag. Other correlations may be zero. A background file operation may have action_id=0. VIEW_READY after composition end may retain the causal composition/snapshot references; this does not say that composition remains active.', '',
           'raw_length, focus and IME/consumed ranges use offsets/counts in the raw UTF-8 byte buffer. Ranges are half-open [start,end). selected_member_count counts product-owned selected members. Document byte ranges refer to the pre-mutation UTF-8 buffer; cursor values are logical UTF-8 byte offsets, never screen coordinates. Selection rank is one-based, global_index/window_start are zero-based. candidate_count_known=UINT32_MAX means unknown. Optional unknown scalar counts/IDs use documented zero or UINT32_MAX, never guessed content.', '',
           'DOCUMENT_MUTATION revision_after equals envelope document_revision. SELECTION_RESULT envelope carries the revision/snapshot against which selection was requested; new_composition_revision/new_focus describe the returned state. Consumed range is meaningful only for Accepted. On rejection, record the actual unchanged revision/focus and an empty consumed range.', '',
           'STATE_CHANGE carries a complete observed structural snapshot, not a guessed diff. SYNC_SNAPSHOT carries the same fields and alone repairs gap trust. candidate_valid=0 means ignore candidate_snapshot_id for current state. A mode change updates layout identity but must not implicitly change Document/composition/candidate identities.', '',
           'All listed payload fields are structurally required and serialized in listed order, with explicit little endian widths. Known BASIC events carry no detail block. DETAILED adds the optional content block from WIRE_FORMAT; every content field is bounded and may be empty. OFF emits nothing. Fields without an applicable enum use integers; modifiers bits are Shift=1, Ctrl=2, Alt=4, Meta=8. Candidate provenance is producer-defined structural source class (0 unknown), never a content key.', '',
           'InputScheme is independent of BaseInputMode. Confirmed source supports FullPinyin, XiaoheShuangpin and English; no unconfirmed Wubi scheme is assigned. EditContext includes Candidate and Literal as explicit state, never inferred from routing. Unknown remains explicit. See evidence/INPUT_SCHEME_SOURCE_NOTE.md.', '',
           'Selection confirmed_output_count=UINT32_MAX and output_count_unit=Unknown means unavailable. Known counts use their declared unit (UnicodeScalar, GraphemeCluster or ProductUnit), never consumed raw bytes. The caller must already know the count; no materialization or expensive scan is authorized.', '',
           'VIEW_READY.reason records the observed cause. Candidate latency requires CandidateUpdate, the same nonzero action/composition/revision/snapshot and a strictly newer layout revision. RUNTIME_IDENTITY mapping and health counter scope are specified in DESIGN_CONTRACT.', '',
           '## Events', '']
    for event in schema['events']:
        lines += [f'### {event["id"]} — {event["name"]}', '', event['purpose'], '',
                  '- C++ payload: `'+event['cpp_name']+'Event`.',
                  '- Required envelope: '+(', '.join(describe_required(rule) for rule in event['required_envelope']) or 'common boot/sequence/time; other identities optional')+'.',
                  '- Required payload, in wire order: '+', '.join('`'+f['name']+':'+f['type']+('('+f['enum']+')' if 'enum' in f else '')+'`' for f in event['fields'])+'.',
                  '- Minimum privacy: '+event['privacy']+'; '+('suppressed entirely in BASIC.' if event['privacy']=='DETAILED' else 'structural fields only in BASIC.'),
                  '- Optional DETAILED content category: '+('`'+event['detail']+'`' if event['detail'] else 'none; leave irrelevant detail fields zero')+'.',
                  '- Producer responsibility: '+event['producer_responsibility'], '']
    lines += ['## Enum values', '', 'u8 enum values below are stable. Zero is the explicit unknown/none/other value where defined; enum numbers are not text encodings.', '']
    for name,values in schema['enums'].items():
        lines += ['- **'+name+'**: '+', '.join(f'`{key}={value}`' for key,value in values.items())+'.']
    lines += ['', '## Generated validation and detail categories', '',
              'ValidateEnvelope.inc is generated solely from required_envelope field/nonzero/when metadata. This is local structural validation, not a check that an identity belongs to a real business lifecycle. Invalid emission is DroppedInvalid before sequence allocation, without a stream gap or SYNC request. BASIC-suppressed detail is excluded before validation. Queue-full valid attempts still consume sequence and mark loss.', '',
              'Every enum field is annotated with its enum type in the payload list and JSON. Host validation reads those annotations; generated ValidatePayload.inc applies the same registry in the emitter. disallowed_enum_values is declarative (e.g. a drawable writing mode cannot be Unknown). Adding a legal enum value does not require editing the decoder.', '',
              'Detail categories: raw = existing raw text; printable = physical printable Unicode; candidate = existing candidate identity/text; document_delta = inserted UTF-8 delta; path = existing spelling/path description. No category authorizes extra business work. All semantic fields without text remain BASIC-safe under the opaque-ID contract.', '',
              'Current layout changes require a new schema revision; append enum values and event IDs without reuse. Unknown future event IDs can be skipped; unsupported schema revisions are explicit errors. See MAINTAINER_GUIDE.', '']
    return '\n'.join(lines)
