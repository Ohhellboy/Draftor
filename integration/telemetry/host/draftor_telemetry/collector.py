"""Raw-first archive writer and POSIX serial/file inputs; no fixed capture duration."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import select
import signal
import time
from .decoder import StreamDecoder
from .reconstruction import SessionReconstructor
from .schema import SCHEMA_REVISION


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


class ArchiveCollector:
    def __init__(self, output, rotate_bytes=1024*1024, source_kind='replay', serial_settings=None, stream_mode='behavior'):
        if rotate_bytes < 1: raise ValueError('rotate_bytes must be positive')
        self.output=Path(output)
        self.output.mkdir(parents=True, exist_ok=True)
        if any(self.output.iterdir()): raise ValueError('output directory must be empty')
        self.limit=rotate_bytes
        if stream_mode not in ('behavior','mixed'): raise ValueError('invalid stream mode')
        self.decoder=StreamDecoder(mixed=stream_mode=='mixed')
        self.parts=[]; self.part=None; self.offset=0
        self.records=(self.output/'decoded.ndjson').open('w', encoding='utf-8')
        self.receives=(self.output/'receives.ndjson').open('w', encoding='utf-8')
        self.diagnostics=(self.output/'diagnostics.ndjson').open('w',encoding='utf-8')
        self.manifest=dict(schema_version=1, schema_revision=SCHEMA_REVISION, wire_version=1, source_kind=source_kind,
                           stream_mode=stream_mode, diagnostics='diagnostics.ndjson',
                           serial_settings=serial_settings, start_host_time=utc_now(),
                           end_host_time=None, stop_reason='running', files=self.parts, bytes=0,
                           receive_index='receives.ndjson', decoded='decoded.ndjson')
        self.closed=False
        self._checkpoint()

    def _checkpoint(self):
        temporary=self.output/'manifest.pending.json'
        write_json(temporary, self.manifest)
        temporary.replace(self.output/'manifest.json')

    def _records(self, records):
        for record in records:
            destination=self.diagnostics if record['record_type']=='diagnostic' else self.records
            destination.write(json.dumps(record, ensure_ascii=False)+'\n')
        self.records.flush(); self.diagnostics.flush()

    def feed(self, data, received_time=None):
        if not data: return
        received_time=received_time or utc_now()
        self.receives.write(json.dumps(dict(record_type='receive',host_received_time=received_time,
                                            stream_offset=self.offset, bytes=len(data)))+'\n')
        self.receives.flush()
        position=0
        while position < len(data):
            if self.part is None or self.parts[-1]['bytes']==self.limit:
                if self.part is not None: self.part.close()
                name=f'part-{len(self.parts)+1:05d}.bin'
                self.part=(self.output/name).open('wb')
                self.parts.append(dict(file=name,bytes=0,start_host_time=received_time,end_host_time=received_time))
            count=min(len(data)-position,self.limit-self.parts[-1]['bytes'])
            self.part.write(data[position:position+count]); self.part.flush()
            self.parts[-1]['bytes']+=count; self.parts[-1]['end_host_time']=received_time
            position+=count
        self.offset+=len(data); self.manifest['bytes']=self.offset
        self._records(self.decoder.feed(data,received_time))
        self._checkpoint()

    def transport_gap(self, reason, received_time=None):
        stamp=received_time or utc_now()
        event=dict(record_type='transport_gap', error=reason, host_received_time=stamp)
        self.receives.write(json.dumps(event)+'\n'); self.receives.flush()
        self._records(self.decoder.finish(stamp,reason)+[event])

    def close(self, reason):
        if self.closed: return
        self.closed=True
        stamp=utc_now()
        self._records(self.decoder.finish(stamp,reason))
        self.records.close(); self.receives.close(); self.diagnostics.close()
        if self.part is not None: self.part.close()
        self.manifest.update(end_host_time=stamp,stop_reason=reason)
        self._checkpoint()


def reconstruct_archive(output):
    output=Path(output); reconstructor=SessionReconstructor()
    with (output/'decoded.ndjson').open(encoding='utf-8') as records:
        for line in records: reconstructor.accept(json.loads(line))
    summary=reconstructor.summary(); write_json(output/'summary.json',summary)
    lines=['# Behavior telemetry basic report', '', 'Evidence: HOST / synthetic or explicitly identified serial input.',
           'Latency uses device monotonic time; host receive time is retained separately.',
           'Counts describe observed reliable intervals, not accuracy or user intent.', '']
    for session in summary['sessions'].values():
        lines += [f'## Boot {session["boot_id"]}', '', f'State: {session["status"]}; history complete: {session["history_complete"]}.',
                  f'Observed actions: {len(session["actions"])}; compositions: {len(session["compositions"])}; gaps: {len(session["gaps"])}.', '',
                  '| Metric | Samples | Mean device µs |', '|---|---:|---:|']
        for name, metric in session['metrics']['latencies'].items():
            lines.append(f'| {name} | {metric["count"]} | {metric["mean_us"]} |')
        lines += ['', 'Workload denominators (raw lengths are UTF-8 input bytes, not output characters):', '',
                  '| Distribution | Samples | Denominator |', '|---|---:|---|']
        for name, denominator in session['metrics']['workload_denominators'].items():
            lines.append(f'| {name} | {denominator["sample_count"]} | {denominator["unit"]} |')
        lines += ['', 'Workload by observed InputScheme: `'+json.dumps(session['metrics']['workload_by_input_scheme'])+'`', '',
                  'Incomplete/ambiguous compositions excluded from peak/terminal samples: `'+json.dumps(session['metrics']['composition_workload_exclusions'])+'`', '']
        other_counts={key:value for key,value in session['metrics'].items()
                      if key not in ('latencies','basis','workload_denominators','workload_by_input_scheme','composition_workload_exclusions')}
        lines += ['Observed distributions/counts: `'+json.dumps(other_counts)+'`', '']
    if summary['unassigned_errors']: lines += ['Unassigned decode/transport errors: '+str(len(summary['unassigned_errors']))]
    (output/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    return summary


def replay(source, collector, chunk_size=4096, stop_requested=lambda: False):
    source=Path(source)
    if source.suffix != '.json':
        with source.open('rb') as stream:
            while True:
                if stop_requested(): return 'ctrl_c'
                data=stream.read(chunk_size)
                if not data: break
                collector.feed(data)
        return 'input_eof'
    manifest=json.loads(source.read_text())
    directory=source.parent
    # Rotation is a byte archive boundary, not a protocol boundary. Read across it.
    def parts():
        for part in manifest['files']:
            with (directory/part['file']).open('rb') as stream:
                while True:
                    data=stream.read(chunk_size)
                    if not data: break
                    yield data
    chunks=iter(parts()); pending=bytearray()
    with (directory/manifest['receive_index']).open() as index:
        for line in index:
            if stop_requested(): return 'ctrl_c'
            receive=json.loads(line)
            if receive['record_type']=='transport_gap':
                collector.transport_gap(receive['error'],receive['host_received_time']); continue
            count=receive['bytes']
            while len(pending)<count:
                try: pending.extend(next(chunks))
                except StopIteration: raise ValueError('archive shorter than receive index')
            collector.feed(bytes(pending[:count]),receive['host_received_time']); del pending[:count]
    if pending or next(chunks,None) is not None: raise ValueError('archive longer than receive index')
    return 'input_eof'


def open_serial(path, baud):
    # POSIX-only adapter. Core and protocol have no OS/serial dependencies.
    import termios
    import tty
    speed=getattr(termios, 'B'+str(baud), None)
    if speed is None: raise ValueError('unsupported POSIX baud rate: '+str(baud))
    descriptor=os.open(path,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    try:
        tty.setraw(descriptor,termios.TCSANOW)
        attrs=termios.tcgetattr(descriptor)
        attrs[4]=speed; attrs[5]=speed
        attrs[2] |= termios.CLOCAL | termios.CREAD
        termios.tcsetattr(descriptor,termios.TCSANOW,attrs)
    except BaseException:
        os.close(descriptor); raise
    return descriptor


def collect_serial(path, baud, collector, reconnect=True, reconnect_delay=1.0, stop_requested=lambda: False):
    descriptor=None
    try:
        while True:
            if stop_requested(): return 'ctrl_c'
            try:
                if descriptor is None:
                    descriptor=open_serial(path,baud)
                readable,_,_=select.select([descriptor],[],[],0.25)
                if not readable: continue
                data=os.read(descriptor,4096)
                if not data: raise OSError('serial_eof')
            except (OSError, ValueError) as error:
                # Archive write/decode failures must not masquerade as serial loss.
                if isinstance(error,ValueError): raise
                collector.transport_gap('serial_disconnect: '+str(error))
                if descriptor is not None: os.close(descriptor); descriptor=None
                if not reconnect: return 'serial_disconnected'
                time.sleep(reconnect_delay)
                continue
            # Disk/archive errors are terminal and must never be handled as a
            # serial reconnect. The raw archive is the authoritative evidence.
            collector.feed(data)
    finally:
        if descriptor is not None: os.close(descriptor)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['replay','serial','analyze'])
    parser.add_argument('--stream-mode',choices=['behavior','mixed'],default=None,help='manifest replay inherits its mode; other input defaults to behavior')
    parser.add_argument('--input', help='raw file or archive manifest for replay')
    parser.add_argument('--port'); parser.add_argument('--baud',type=int,default=115200)
    parser.add_argument('--output',required=True)
    parser.add_argument('--rotate-bytes',type=int,default=1024*1024)
    parser.add_argument('--no-reconnect',action='store_true')
    parser.add_argument('--reconnect-delay',type=float,default=1.0)
    args=parser.parse_args(argv)
    if args.mode=='analyze': reconstruct_archive(args.output); return 0
    if args.mode=='serial' and not args.port: parser.error('--port is required')
    if args.mode=='replay' and not args.input: parser.error('--input is required')
    if args.reconnect_delay < 0: parser.error('--reconnect-delay must be nonnegative')
    settings=dict(port=args.port,baud=args.baud,data_bits=8,parity='none',stop_bits=1,reconnect=not args.no_reconnect) if args.mode=='serial' else None
    stream_mode=args.stream_mode
    if stream_mode is None and args.mode=='replay' and Path(args.input).suffix=='.json':
        stream_mode=json.loads(Path(args.input).read_text()).get('stream_mode','behavior')
    collector=ArchiveCollector(args.output,args.rotate_bytes,args.mode,settings,stream_mode or 'behavior')
    reason='error'; exit_code=0
    stopping=False
    def request_stop(_signal, _frame):
        nonlocal stopping
        stopping=True
    # Finish the current archive chunk before shutdown; SIGINT must not split
    # the raw write, receive index, and manifest update into inconsistent pieces.
    previous_handler=signal.signal(signal.SIGINT,request_stop)
    try:
        if args.mode=='replay': reason=replay(args.input,collector,stop_requested=lambda:stopping)
        else: reason=collect_serial(args.port,args.baud,collector,not args.no_reconnect,args.reconnect_delay,lambda:stopping)
    except KeyboardInterrupt:
        reason='ctrl_c'
    except Exception as error:
        reason='error: '+str(error); exit_code=1
        print(reason,flush=True)
    finally:
        try: collector.close(reason)
        finally: signal.signal(signal.SIGINT,previous_handler)
    # Analysis is explicitly separate so long-lived capture uses bounded memory.
    if args.mode=='replay': reconstruct_archive(args.output)
    return exit_code

if __name__=='__main__': raise SystemExit(main())
