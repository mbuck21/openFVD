#!/usr/bin/env python3
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
"""Record NoLimits 2's telemetry to a CSV file (research note R12, section 5.2).

    py -3 tools/nl2log.py --probe
    py -3 tools/nl2log.py --out samples/private/e1-v40-car5.csv [--seat CAR SEAT]
    py -3 tools/nl2log.py --selftest

The protocol is the one on NoLimits 2's help page "Telemetry Interface" (TCP,
default port 15151, switched on in Setup; all numbers big-endian). A message
is 'N', a 2-byte type, a 4-byte request id, a 2-byte data size, the data and
'L'. This tool uses four message types: Get Version (3, reply 4), Get
Telemetry (5, reply 6 with 76 bytes of data), Select Seat (29, 16 bytes of
data: coaster, train, car, seat). The help page says every message is
answered in order and lists the server's replies, among them OK (type 1, no
data) and Error (type 2, a UTF-8 text). For Select Seat it names no reply, so
a reply to it is tolerated either way: when a request waits for its reply, any
message with an older request id is a late answer to a request nobody waited
for and is skipped (counted, at most 16 in a row); an Error with this
request's id stops with the server's text. An Error skipped that way would be
a refused Select Seat, so right after Select Seat the recorder makes one round
trip that waits for a reply (Get Version) and stops with the server's text if
an Error came back; it then reads one telemetry frame and compares its car and
seat with the request. A difference only prints a warning (whether car and
seat are counted from 0 is a "check on screen" item), and both the requested
and the reported numbers go into the CSV's comment lines. Everything is logged as the server sends it:
the help page does not say which unit speed, position or g are in (that is
what experiment E0 finds out), so no conversion is done here.

The CSV has one header line and one row per new rendered frame (a frame number
that has not been seen is new data), with `t_s`, the seconds since the first
row on this computer's clock (the telemetry has no time stamp, R12 section 5.2).
Standard library only. The self-test starts a fake server on 127.0.0.1 on a
port the system picks and closes it again; it never touches NoLimits 2.
"""
import argparse
import csv
import socket
import struct
import sys
import threading
import time
from pathlib import Path

MAGIC_START = b'N'
MAGIC_END = b'L'
T_GET_VERSION, T_VERSION = 3, 4
T_GET_TELEMETRY, T_TELEMETRY = 5, 6
T_SELECT_SEAT = 29
T_OK, T_ERROR = 1, 2
MAX_SKIP = 16
TELEMETRY_SIZE = 76
DEFAULT_PORT = 15151

COLUMNS = ['t_s', 'frame', 'flags', 'view', 'coaster', 'style', 'train',
           'car', 'seat', 'speed', 'x', 'y', 'z', 'qx', 'qy', 'qz', 'qw',
           'gx', 'gy', 'gz']
# data layout of the telemetry reply: 8 int32, then 11 float32 (76 bytes)
TELEMETRY_FORMAT = '>8i11f'


class ProtocolError(Exception):
    pass


def pack_message(type_id, request_id, data=b''):
    return (MAGIC_START + struct.pack('>HIH', type_id, request_id, len(data))
            + data + MAGIC_END)


def read_exact(sock, n):
    chunks = []
    while n > 0:
        chunk = sock.recv(n)
        if not chunk:
            raise ProtocolError('the connection was closed')
        chunks.append(chunk)
        n -= len(chunk)
    return b''.join(chunks)


def read_message(sock):
    """Returns (type_id, request_id, data)."""
    head = read_exact(sock, 9)
    if head[:1] != MAGIC_START:
        raise ProtocolError('a message did not start with N')
    type_id, request_id, size = struct.unpack('>HIH', head[1:])
    data = read_exact(sock, size)
    if read_exact(sock, 1) != MAGIC_END:
        raise ProtocolError('a message did not end with L')
    return type_id, request_id, data


class Client:
    def __init__(self, host, port, timeout=5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.next_id = 1
        self.skipped = []       # types of replies skipped as late
        self.skipped_errors = []  # texts of Error replies among them

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def request(self, type_id, data=b'', want=None):
        rid = self.next_id
        self.next_id += 1
        self.sock.sendall(pack_message(type_id, rid, data))
        if want is None:
            return None
        skipped_now = 0
        while True:
            got_type, got_id, got = read_message(self.sock)
            if got_id < rid:
                # a late answer to a request nobody waited for (Select Seat)
                self.skipped.append(got_type)
                if got_type == T_ERROR:
                    self.skipped_errors.append(
                        got.decode('utf-8', 'replace'))
                skipped_now += 1
                if skipped_now > MAX_SKIP:
                    raise ProtocolError('more than %d unexpected replies in '
                                        'a row' % MAX_SKIP)
                continue
            break
        if got_id != rid:
            raise ProtocolError('reply %d answers request %d, not %d'
                                % (got_type, got_id, rid))
        if got_type == T_ERROR and want != T_ERROR:
            raise ProtocolError('the server answered with an error: %s'
                                % got.decode('utf-8', 'replace'))
        if got_type != want:
            raise ProtocolError('expected a reply of type %d, got %d'
                                % (want, got_type))
        return got

    def version(self):
        data = self.request(T_GET_VERSION, want=T_VERSION)
        return tuple(data)          # 4 bytes; their meaning is not stated

    def telemetry(self):
        data = self.request(T_GET_TELEMETRY, want=T_TELEMETRY)
        if len(data) != TELEMETRY_SIZE:
            raise ProtocolError('telemetry data is %d bytes, not %d'
                                % (len(data), TELEMETRY_SIZE))
        return struct.unpack(TELEMETRY_FORMAT, data)

    def select_seat(self, coaster, train, car, seat):
        # The help page names no reply for this message, so none is waited
        # for. If the server does answer (OK, or Error), the answer is skipped
        # by request id when the next request waits for its own reply.
        self.request(T_SELECT_SEAT, struct.pack('>4i', coaster, train, car,
                                                seat))


def select_seat_checked(cli, coaster, train, car, seat, log=print):
    """Select a seat and check the answer. Returns a note for the CSV.

    Raises ProtocolError with the server's text when the server refused. A
    telemetry frame that reports another car or seat only logs a warning."""
    cli.select_seat(coaster, train, car, seat)
    cli.version()       # a round trip: the answer to Select Seat is in by now
    if cli.skipped_errors:
        raise ProtocolError('the server refused Select Seat: '
                            + '; '.join(cli.skipped_errors))
    vals = cli.telemetry()
    got = (vals[3], vals[5], vals[6], vals[7])
    want = (coaster, train, car, seat)
    note = ('requested coaster %d train %d car %d seat %d; telemetry reports '
            'coaster %d train %d car %d seat %d' % (want + got))
    if got != want:
        log('WARNING: you asked for coaster %d train %d car %d seat %d but '
            'telemetry reports coaster %d train %d car %d seat %d. The '
            'numbering may start at 1, or the seat is not selectable yet '
            '(for example before the ride starts); recording goes on.'
            % (want + got))
    return note


def telemetry_row(values, t_s):
    """values: the 19 unpacked fields; returns a dict keyed by COLUMNS."""
    (flags, frame, view, coaster, style, train, car, seat) = values[:8]
    rest = values[8:]
    row = dict(t_s=t_s, flags=flags, frame=frame, view=view, coaster=coaster,
               style=style, train=train, car=car, seat=seat)
    for key, v in zip(COLUMNS[9:], rest):
        row[key] = v
    return row


def record(client, out_path, rate_hz=100.0, duration=None, start_speed=0.1,
           stop_after_frames=0, wait_for_play=True, log=print,
           max_polls=None, note=''):
    """Poll telemetry and write new frames to out_path. Returns the row count.

    Recording starts when the speed first reaches start_speed (raw unit) and
    ends after `duration` seconds, or when the speed has been below
    start_speed for stop_after_frames new frames after having been above it
    (0 = never), or on Ctrl+C. Rows are flushed as they are written."""
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    period = 1.0 / rate_hz if rate_hz > 0 else 0.0
    rows = 0
    seen = None
    t0 = None
    started = False
    slow = 0
    polls = 0
    with open(out_path, 'w', newline='') as fh:
        fh.write('# nl2log: raw telemetry as NoLimits 2 sends it; units of '
                 'speed, position and g are not stated by its help\n')
        if note:
            fh.write('# seat: ' + note + '\n')
        wr = csv.DictWriter(fh, fieldnames=COLUMNS)
        wr.writeheader()
        try:
            while True:
                tick = time.monotonic()
                values = client.telemetry()
                polls += 1
                frame, flags, speed = values[1], values[0], values[8]
                if frame != seen and not (wait_for_play and not flags & 1):
                    seen = frame
                    if not started and abs(speed) >= start_speed:
                        started = True
                        t0 = tick
                        log('recording started at frame %d' % frame)
                    if started:
                        wr.writerow(telemetry_row(
                            (flags,) + values[1:], tick - t0))
                        rows += 1
                        if abs(speed) < start_speed:
                            slow += 1
                        else:
                            slow = 0
                        if stop_after_frames and slow >= stop_after_frames:
                            log('the train has stopped; recording ended')
                            break
                        if rows % 500 == 0:
                            fh.flush()
                if started and duration is not None and tick - t0 >= duration:
                    break
                if max_polls is not None and polls >= max_polls:
                    break
                left = period - (time.monotonic() - tick)
                if left > 0:
                    time.sleep(left)
        except KeyboardInterrupt:
            log('stopped by Ctrl+C')
    return rows


# ------------------------------------------------------------ self-test --
class FakeServer:
    """A stand-in for NoLimits 2's telemetry server, for the self-test.

    It follows the help page's framing. Simulated train: speed v0 falls
    linearly with the frame number; the frame number advances every second
    request, so the recorder sees repeated frames and must skip them. It
    starts in play mode after `idle` requests, and honours Select Seat."""

    def __init__(self, v0=20.0, frames=300, idle=5, reply_seat=False,
                 error_text=None, seat_error=None, ignore_seat=False):
        self.v0, self.frames, self.idle = v0, frames, idle
        self.reply_seat = reply_seat        # answer Select Seat with OK
        self.error_text = error_text        # answer Get Telemetry with Error
        self.seat_error = seat_error        # answer Select Seat with Error
        self.ignore_seat = ignore_seat      # report another seat than asked
        self.seat = (0, 0, 0, 0)
        self.requests = []
        self.sock = socket.socket()
        self.sock.bind(('127.0.0.1', 0))
        self.sock.listen(1)
        self.port = self.sock.getsockname()[1]
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _telemetry(self, n):
        if n < self.idle:
            return struct.pack(TELEMETRY_FORMAT, 0, 0, 0, 0, 0, 0, 0, 0,
                               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
                               0.0, 1.0, 0.0)
        frame = 1000 + (n - self.idle) // 2
        k = frame - 1000
        speed = max(self.v0 - 0.05 * k, 0.0)
        pos = sum(max(self.v0 - 0.05 * i, 0.0) / 60.0 for i in range(k))
        coaster, train, car, seat = self.seat
        return struct.pack(TELEMETRY_FORMAT, 1, frame, 1, coaster, 7, train,
                           car, seat, speed, pos, 5.0, 0.0, 0.0, 0.0, 0.0,
                           1.0, 0.0, 1.0, 0.0)

    def _serve(self):
        try:
            conn, _ = self.sock.accept()
        except OSError:
            return
        with conn:
            n = 0
            while True:
                try:
                    type_id, rid, data = read_message(conn)
                except (ProtocolError, OSError):
                    return
                self.requests.append(type_id)
                if type_id == T_GET_TELEMETRY and self.error_text:
                    conn.sendall(pack_message(
                        T_ERROR, rid, self.error_text.encode('utf-8')))
                elif type_id == T_GET_TELEMETRY:
                    conn.sendall(pack_message(T_TELEMETRY, rid,
                                              self._telemetry(n)))
                    n += 1
                elif type_id == T_GET_VERSION:
                    conn.sendall(pack_message(T_VERSION, rid, bytes([2, 0, 3, 1])))
                elif type_id == T_SELECT_SEAT:
                    if self.seat_error:
                        conn.sendall(pack_message(
                            T_ERROR, rid, self.seat_error.encode('utf-8')))
                        continue
                    if not self.ignore_seat:
                        self.seat = struct.unpack('>4i', data)
                    if self.reply_seat:
                        conn.sendall(pack_message(T_OK, rid))
                else:
                    return

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass
        self.thread.join(timeout=2.0)


def selftest():
    import tempfile
    failures = []

    def check(cond, text):
        print(('ok   ' if cond else 'FAIL ') + text)
        if not cond:
            failures.append(text)

    srv = FakeServer()
    port = srv.port
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'rec.csv'
        cli = Client('127.0.0.1', port)
        try:
            check(cli.version() == (2, 0, 3, 1), 'Get Version round trip')
            cli.select_seat(0, 0, 3, 1)
            rows = record(cli, out, rate_hz=0, start_speed=0.1,
                          stop_after_frames=20, log=lambda *_: None,
                          max_polls=5000)
        finally:
            cli.close()
        with open(out, newline='') as fh:
            lines = [ln for ln in fh if not ln.startswith('#')]
        data = list(csv.DictReader(lines))
        check(rows == len(data) and rows > 100, 'rows written: %d' % rows)
        frames = [int(d['frame']) for d in data]
        check(frames == sorted(set(frames)), 'every frame once, in order '
              '(repeated frames skipped)')
        check(frames[0] == 1000 and frames[-1] - frames[0] + 1 == len(frames),
              'no frame lost')
        check(all(d['flags'] == '1' for d in data), 'play mode only')
        check(all(d['car'] == '3' and d['seat'] == '1' for d in data),
              'Select Seat reached the server (car 3, seat 1)')
        speeds = [float(d['speed']) for d in data]
        check(abs(speeds[0] - 20.0) < 1e-3
              and all(a >= b for a, b in zip(speeds, speeds[1:])),
              'speed read back and falling')
        ts = [float(d['t_s']) for d in data]
        check(ts[0] == 0.0 and ts == sorted(ts), 't_s starts at 0 and rises')
        check(abs(float(data[60]['x']) - sum(
            max(20.0 - 0.05 * i, 0.0) / 60.0 for i in range(60))) < 1e-3,
            'position read back')
        check(T_GET_TELEMETRY in srv.requests, 'server saw Get Telemetry')

    # Select Seat answered with OK: the late answer must be skipped
    srv2 = FakeServer(reply_seat=True)
    cli = Client('127.0.0.1', srv2.port)
    try:
        cli.select_seat(0, 0, 3, 1)
        cli.select_seat(0, 0, 4, 1)
        for _ in range(7):         # the fake server idles for 5 requests
            vals = cli.telemetry()
        check(cli.skipped == [T_OK, T_OK], 'late OK replies to Select Seat '
              'skipped (%s)' % cli.skipped)
        check(vals[6] == 4 and vals[7] == 1, 'telemetry read after them '
              '(car 4, seat 1)')
    except ProtocolError as e:
        check(False, 'Select Seat with an OK reply: %s' % e)
    finally:
        cli.close()
        port2 = srv2.port
        srv2.close()

    # Get Telemetry answered with Error: the server's text is reported
    srv3 = FakeServer(error_text='no coaster loaded')
    cli = Client('127.0.0.1', srv3.port)
    try:
        cli.telemetry()
        check(False, 'an Error reply raises')
    except ProtocolError as e:
        check('no coaster loaded' in str(e),
              'an Error reply raises with the server text (%s)' % e)
    finally:
        cli.close()
        port3 = srv3.port
        srv3.close()

    # record mode: Select Seat refused with Error stops with the text
    srv4 = FakeServer(idle=0, seat_error='seat 9 does not exist')
    cli = Client('127.0.0.1', srv4.port)
    try:
        select_seat_checked(cli, 0, 0, 9, 0, log=lambda *_: None)
        check(False, 'a refused Select Seat stops the recording')
    except ProtocolError as e:
        check('seat 9 does not exist' in str(e),
              'a refused Select Seat stops with the server text (%s)' % e)
    finally:
        cli.close()
        port4 = srv4.port
        srv4.close()

    # record mode: another car reported -> a warning, and recording goes on
    srv5 = FakeServer(idle=0, ignore_seat=True)
    cli = Client('127.0.0.1', srv5.port)
    warnings = []
    with tempfile.TemporaryDirectory() as tmp:
        out5 = Path(tmp) / 'seat.csv'
        try:
            note = select_seat_checked(cli, 0, 0, 3, 1, log=warnings.append)
            n5 = record(cli, out5, rate_hz=0, start_speed=0.1,
                        stop_after_frames=20, log=lambda *_: None,
                        max_polls=5000, note=note)
        finally:
            cli.close()
            port5 = srv5.port
            srv5.close()
        head = out5.read_text().splitlines()[:3]
        check(len(warnings) == 1 and 'car 3 seat 1' in warnings[0]
              and 'car 0 seat 0' in warnings[0],
              'another reported seat prints a warning naming both')
        check(n5 > 100, 'and the recording went on (%d rows)' % n5)
        check(head[1].startswith('# seat: requested')
              and 'car 3 seat 1' in head[1] and 'car 0 seat 0' in head[1],
              'requested and reported seat are in the CSV comment line')

    # a stream of nothing but old replies still ends in an error
    a_, b_ = socket.socketpair()
    for i in range(MAX_SKIP + 2):
        a_.sendall(pack_message(T_OK, 0))
    cl = Client.__new__(Client)
    cl.sock, cl.next_id, cl.skipped, cl.skipped_errors = b_, 5, [], []
    try:
        cl.request(T_GET_TELEMETRY, want=T_TELEMETRY)
        check(False, 'endless old replies end in an error')
    except ProtocolError:
        check(True, 'endless old replies end in an error')
    a_.close()
    b_.close()

    # bad framing is reported, not swallowed
    a, b = socket.socketpair()
    a.sendall(b'X' + bytes(9))
    try:
        read_message(b)
        check(False, 'bad start byte refused')
    except ProtocolError:
        check(True, 'bad start byte refused')
    a.close()
    b.close()

    srv.close()
    # nothing of ours may still be listening
    for pt in (port, port2, port3, port4, port5):
        try:
            socket.create_connection(('127.0.0.1', pt), timeout=1.0).close()
            check(False, 'the fake server is closed')
        except OSError:
            check(True, 'the fake server is closed (port %d refuses)' % pt)
    print('PASS' if not failures else 'FAIL (%d)' % len(failures))
    return 0 if not failures else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--port', type=int, default=DEFAULT_PORT)
    ap.add_argument('--out', help='CSV file to write (under samples/private/)')
    ap.add_argument('--rate', type=float, default=120.0,
                    help='polls per second (default 120; 0 = as fast as it answers)')
    ap.add_argument('--duration', type=float, default=None,
                    help='seconds to record after the train starts moving')
    ap.add_argument('--start-speed', type=float, default=0.1,
                    help='record from the first speed at or above this '
                         '(raw telemetry unit)')
    ap.add_argument('--stop-after', type=int, default=180,
                    help='end after this many new frames below the start '
                         'speed (default 180; 0 = only Ctrl+C or --duration)')
    ap.add_argument('--seat', nargs=2, type=int, metavar=('CAR', 'SEAT'),
                    help='select this car and seat (0-based) first, through '
                         'Select Seat')
    ap.add_argument('--coaster', type=int, default=0)
    ap.add_argument('--train', type=int, default=0)
    ap.add_argument('--any-mode', action='store_true',
                    help='also record frames when play mode is off')
    ap.add_argument('--probe', action='store_true',
                    help='connect, print the version and one telemetry frame, '
                         'and stop')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()

    if args.selftest:
        sys.exit(selftest())
    try:
        cli = Client(args.host, args.port)
    except OSError as e:
        sys.exit('cannot connect to %s:%d (%s). Is the telemetry server '
                 'switched on in NoLimits 2?' % (args.host, args.port, e))
    try:
        if args.probe:
            print('version bytes:', cli.version())
            vals = cli.telemetry()
            print('late replies skipped: %d %s' % (
                len(cli.skipped), cli.skipped))
            for name, v in zip(['flags', 'frame', 'view', 'coaster', 'style',
                                'train', 'car', 'seat', 'speed', 'x', 'y', 'z',
                                'qx', 'qy', 'qz', 'qw', 'gx', 'gy', 'gz'],
                               vals):
                print('%-8s %s' % (name, v))
            return
        if not args.out:
            sys.exit('give --out <file.csv> (use a path under samples/private/)')
        note = ''
        if args.seat:
            try:
                note = select_seat_checked(cli, args.coaster, args.train,
                                           args.seat[0], args.seat[1])
            except ProtocolError as e:
                sys.exit('error: %s' % e)
        print('waiting for the train to move; press Ctrl+C to stop')
        n = record(cli, args.out, rate_hz=args.rate, duration=args.duration,
                   start_speed=args.start_speed,
                   stop_after_frames=args.stop_after,
                   wait_for_play=not args.any_mode, note=note)
        print('wrote %d rows to %s' % (n, args.out))
    finally:
        cli.close()


if __name__ == '__main__':
    main()
