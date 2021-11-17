#!/usr/bin/env python3

from enum import Enum
import struct
import time

import serial


class FrameAddress(Enum):
    '''Allowable addresses for packets we can send/receive'''
    DEVICE = b'd'
    DUT = b't'
    CONTROL = b'C'
    LOGGING = b'L'

class Frame:
    def __init__(self, address: FrameAddress, control: int, payload: bytes):
        self.address = address
        self.control = control
        self.payload = payload

    def frame(self):
        pass


class Framer:
    '''Implements the serial level line framing'''
    FLAG = b'~'
    ESCAPE = b'}'

    @classmethod
    def frame(cls, payload: bytes, address = FrameAddress.DEVICE, control = 0) -> bytes:
        if -1 == payload.find(cls.FLAG) and -1 == payload.find(cls.ESCAPE):
            escaped_payload = payload
        else:
            escaped_payload = bytearray()
            for b in payload:
                # print("@ ", b, ord(cls.FLAG))
                if b in [ord(cls.FLAG), ord(cls.ESCAPE)]:
                    escaped_payload.append(ord(cls.ESCAPE))
                escaped_payload.append(b)
        l = len(escaped_payload)

        escaped_frame = bytearray()
        def _enframe(b):
            if b in [ord(cls.FLAG), ord(cls.ESCAPE)]:
                escaped_frame.append(ord(cls.ESCAPE))
            escaped_frame.append(b)

        _enframe(address.value[0])
        _enframe(control)
        _enframe((l & 0xFF00) >> 8)
        _enframe((l & 0xFF))
        escaped_frame.extend(escaped_payload)

        cksum = Framer.fcs(escaped_frame)
        _enframe((cksum & 0xFF00) >> 8)
        _enframe((cksum & 0xFF))

        result = bytearray()
        result.append(ord(cls.FLAG))
        result.extend(escaped_frame)
        result.append(ord(cls.FLAG))
        return bytes(result)

    @classmethod
    def fcs(cls, buf: bytes):
        crc = 0xFFFF

        for x in buf:
            for i in range(8):
                crc= (crc>>1)^0x8408 if ((crc&1)^(x&1)) else crc>>1
                x >>= 1
        crc = crc & 0xffff
        return crc


class RXState(Enum):
    IDLE = 1
    WAIT_ADDR = 2
    WAIT_CONTROL = 3
    WAIT_LEN_HI = 4
    WAIT_LEN_LO = 5
    IN_BODY = 6
    ESCAPE_FOUND = 7
    WAIT_CKSUM_HI = 8
    WAIT_CKSUM_LO = 9

class SerialRX:
    MAX_PACKET_LEN = 1500

    def __init__(self, cb):
        '''Set up a SerialRX state machine

        cb is a callback which takes a Frame instance as an argument
        '''
        self.state = RXState.IDLE
        self.cb = cb

        self._reset_state()

    def _reset_state(self):
        '''Resets the state ofthe parser'''
        self.accumulator = []

        self.cur_addr = None
        self.cur_control = None
        self.cur_len = None
        self.cur_cksum = None

        self.body_rem = None  # Bytes left in the body of the current frame

        self.saw_escape = False

    def rx_byte(self, b):
        is_escape = (b == ord(Framer.ESCAPE))
        is_flag = (b == ord(Framer.FLAG))

        if not self.saw_escape and is_escape:
            self.saw_escape = True
            if self.state == RXState.IN_BODY:
                # Escapes in body count toward frame length
                self.body_rem -= 1
            return

        # print(". ", self.state, b, is_escape, is_flag)

        if self.state == RXState.IDLE:
            if not is_flag:
                # Waiting for a flag; let the connection idle
                return
            self.state = RXState.WAIT_ADDR
            return

        if self.state == RXState.WAIT_ADDR:
            if is_flag:
                # We allow the channel to idle at ~~~~~~
                return
            self.cur_addr = b
            self.state = RXState.WAIT_CONTROL
            return

        if self.state == RXState.WAIT_CONTROL:
            self.cur_control = b
            self.state = RXState.WAIT_LEN_HI
            return

        # Get our lengths
        if self.state == RXState.WAIT_LEN_HI:
            self.cur_len = b << 8
            self.state = RXState.WAIT_LEN_LO
            return

        if self.state == RXState.WAIT_LEN_LO:
            self.cur_len += b
            self.body_rem = self.cur_len

            if self.cur_len > SerialRX.MAX_PACKET_LEN:
                self.state = RXState.IDLE
                raise ValueError(f'Packet is too long: {self.cur_len} > {SerialRX.MAX_PACKET_LEN}')

            self.state = RXState.IN_BODY
            return

        # Wait for the body and possibly its end

        if self.state == RXState.IN_BODY:
            self.body_rem -= 1
            if is_escape:
                self.state = RXState.ESCAPE_FOUND
                return
            self.accumulator.append(b)
            # fall through to escape_found case,

        if self.state == RXState.ESCAPE_FOUND:
            # We blindly pass through anything we get after an escape
            self.accumulator.append(b)
            self.body_rem -= 1
            self.state = RXState.IN_BODY

        if self.state == RXState.IN_BODY and self.body_rem == 0:
            self.state = RXState.WAIT_CKSUM_HI
            return

        if self.state == RXState.WAIT_CKSUM_HI:
            self.cur_cksum = b << 8
            self.state = RXState.WAIT_CKSUM_LO
            return

        if self.state == RXState.WAIT_CKSUM_LO:
            self.cur_cksum += b
            buf = bytes(self.accumulator)
            # TODO Check checksum
            f = Frame(self.cur_addr, self.cur_control, buf)
            self.cb(f)
            self.state = RXState.IDLE
            self._reset_state()

    def rx(self, buf):
        for b in buf:
            self.rx_byte(b)



class ArgaliConsole:

    def __init__(self, s):
        self.s = s
        self.rx = SerialRX(self.frame_cb)

        self.last_bytes = []  # Accumulator for bytes, used do debug bad frames

        self.pending_frames = []  # A list of frames to send

        self.dump_serial_inbound = False

        self.pending_echo = False
        self.last_echo_sent = None

    def tx(self, bs):
        #print(f'     {len(bs):4d} >> {bs}')
        return self.s.write(bs)

    def poll(self):
        buf = s.read(10)
        if buf and self.dump_serial_inbound:
            print(f'<< {buf}')
        self.last_bytes.extend(buf)
        try:
            self.rx.rx(buf)
        except Exception as e:
            print(f'Exception {e}: bytes={self.last_bytes}')
            self.last_bytes = []

        if self.pending_frames:
            f = self.pending_frames.pop(0)
            #print(f'Need to send a frame: {f}')
            #print(f'    >> Sending frame len={len(f)}')
            n = self.tx(b'~~~')
            #print(f'Preamble sent: {n}')
            n += self.tx(f)
            #print(f'Payload sent: {n}-3/{len(f)}')
            if len(f) % 8:
                self.tx(b'~' * (8 - (n%8)))

    def enqueue(self, f):
        #print(f'Enqueueing frame: {f}')
        self.pending_frames.append(f)

    def frame_cb(self, f):
        self.last_bytes = []

        if f.address == ord('L'):
            print(f'                                         Logline: {f.payload.decode()}')
            return

        print(f'     Got frame: {f.address}/{f.control}: {f.payload[0]}')

        if f.payload[0] == ord('E'):
            print(f'Handling echo')
            self._echo_rx(f.payload)


    def echo(self, s, rq="Q"):
        '''Request the remote side echo back a blob of data'''
        payload = f'E{rq}{s}'.encode('ISO8859-1')
        self.enqueue(Framer.frame(payload))
        self.pending_echo = True
        self.last_echo_sent = time.time()

    def _echo_rx(self, payload):
        '''Handles inbound echo packets'''

        print(f'Got an echo packet inbound: {payload[0]}/{payload[1]}')
        if payload[0] != ord('E'):
            raise ValueError(f'Called echo rx on a packet with type "{payload[0]}" != "E": {payload}')
        qr = payload[1]
        if qr == ord("R"):
            print(f'Got echo Response: {payload[2:]}')
            self.pending_echo = False
            self.last_echo_sent = None

        elif qr == ord("Q"):
            print(f'Got an echo request: {payload[2:]}, enqueuing response')
            self.echo(payload[2:], "R")

    def reset_req(self):
        '''Request the remote target to reset itself'''



s = serial.Serial("/dev/ttyACM0", 115200, timeout=0)
ac = ArgaliConsole(s)

n_loops = 0

while True:
    ac.poll()
    n_loops += 1

    if 0 == (n_loops % 100):
        if not ac.pending_echo or (time.time() - ac.last_echo_sent) > 5:
            print('saying hi')
            ac.echo(f"Hi there ~ {{ }} ~ ~{n_loops}")
    time.sleep(0.01)
