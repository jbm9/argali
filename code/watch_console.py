#!/usr/bin/env python3

from enum import Enum
import struct
import time

import sys

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
            self.accumulator.append(b)
            # fall through to escape_found case,

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
        self.pending_dac = True
        self.pending_adc_bytes = 0

        self.last_echo_sent = None

    def pending_input(self):
        if self.pending_echo or self.pending_dac or (self.pending_adc_bytes > 0):
            return True

    def tx(self, bs):
        #print(f'     {len(bs):4d} >> {bs}')
        n = self.s.write(bs)
        if n != len(bs):
            print(f'Partial write')
        self.s.flush()
        return n


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
                self.tx(b'~' * 8)
            self.s.flush()
        else:
            self.tx(b'~' * 1)

    def enqueue(self, f):
        #print(f'Enqueueing frame: {f}')
        self.pending_frames.append(f)

    def frame_cb(self, f):
        self.last_bytes = []

        #print(f'     Got frame: {f.address}/{f.control}: {f.payload[0]}')

        if f.address == ord('L'):
            return self._logline(f)

        handlers = {
            ord('!'): self._error,
            ord('E'): self._echo_rx,
            ord('D'): self._dac_rx,
            ord('A'): self._adc_rx,
            }

        family = f.payload[0]

        handler = handlers.get(family, self._unknown_family)
        return handler(f)

    def _unknown_family(self, f):
        print(f'Unknown payload: {f.payload[0]}/{f.payload[1]} {f.payload[2:].decode("iso8859-1")}')

    def _logline(self, f):
            print(f'                                         Logline({f.control}): {f.payload.decode("iso8859-1")}')

    def _error(self, f):
            print(f'Got an error packet: {f.payload.decode("iso8859-1")}')

    def echo(self, s, rq="Q"):
        '''Request the remote side echo back a blob of data'''
        payload = f'E{rq}{s}'.encode('ISO8859-1')
        self.enqueue(Framer.frame(payload))
        self.pending_echo = True
        self.last_echo_sent = time.time()

    def _echo_rx(self, f):
        '''Handles inbound echo packets'''

        payload = f.payload
        #print(f'Got an echo packet inbound: {payload[0]}/{payload[1]}')
        if payload[0] != ord('E'):
            raise ValueError(f'Called echo rx on a packet with type "{payload[0]}" != "E": {payload}')
        qr = payload[1]
        if qr == ord("R"):
            print(f'Got echo Response: {payload[2:]}')
            self.pending_echo = False
            self.last_echo_sent = None
        elif qr == ord('U'):
            print(f'Got full value table: {len(payload)}')

            self.pending_echo = False
            self.last_echo_sent = None

            for i in range(2, len(payload), 16):
                print(f'{i-2:4d}: {payload[i:i+16].hex()}')


        elif qr == ord("Q"):
            print(f'Got an echo request: {payload[2:]}, enqueuing response')
            self.echo(payload[2:], "R")
        else:
            return self._unknown_family(f)

    def reset_req(self):
        '''Request the remote target to reset itself'''
        payload = b'RQ'
        self.enqueue(Framer.frame(payload))


    def dac_config_req(self, prescaler, period, scale, ppw, num_waves):
        '''Request a DAC setup'''
        payload = struct.pack('>ccHLBHB', b"D",b"C", prescaler, period, scale, ppw, num_waves)
        self.enqueue(Framer.frame(payload))
        self.pending_dac = True

    def dac_start_req(self):
        self.enqueue(Framer.frame(b"DS"))
        self.pending_dac = True

    def dac_stop_req(self):
        self.enqueue(Framer.frame(b"DT"))
        self.pending_dac = True

    def _dac_rx(self, f):
        print(f'Got DAC reply')
        self.pending_dac = False


    def adc_capture_req(self, prescaler, period, num_points, sample_width, n_channels, channels):
        '''Request an ADC capture on the given channels'''
        payload = struct.pack(f'>ccHLHBB{n_channels}B', b"A",b"C", prescaler, period, num_points, sample_width, n_channels, *channels)
        print(payload)
        self.enqueue(Framer.frame(payload))
        self.pending_adc_bytes = num_points * sample_width * n_channels
        print(f"Submitted ADC request for {self.pending_adc_bytes} bytes")

    def _adc_rx(self, f):
        '''Handles inbound ADC packets'''

        payload = f.payload
        if payload[0] != ord('A'):
            raise ValueError(f'Called echo rx on a packet with type "{payload[0]}" != "E": {payload}')
        qr = payload[1]
        if qr == ord("C"):
            self.pending_adc_bytes -= (len(payload)-2)
            print(f'Got ADC Data: ({self.pending_adc_bytes} after this)')
            for i in range(2, len(payload), 16):
                print(f'{i-2:4d}: {payload[i:i+16].hex()}')

    def idle(self, n):
        self.tx(b'~' * n)

    def bogon(self):
        self.enqueue(Framer.frame(b'xxfoo'))


################################################################################


def _pend():
    while ac.pending_input():
        #print(".")
        ac.poll()
        time.sleep(0.01)

def _wait(s):
    print(f'Round-trippping: {s}')
    ac.echo(s)
    ac.poll()
    while ac.pending_echo:
        ac.poll()
        time.sleep(0.01)



######################################################################

s = serial.Serial("/dev/ttyACM0", 115200, timeout=0)
ac = ArgaliConsole(s)

print('Starting up')

print('Configure DAC')
ac.dac_config_req(9,1,2,125,1)
_pend()

print('Starting DAC')
ac.dac_start_req()
_pend()

print('Starting up ADC')
ac.adc_capture_req(104, 49, 100, 2, 2, b'\x02\x00')
_pend()


print('Stopping DAC')
ac.dac_stop_req()
_pend()


print('Exiting')
sys.exit(1)

############################################################

n_loops = 0
while True:
    ac.poll()
    n_loops += 1

    if 0 == (n_loops % 100):
        if not ac.pending_echo or (time.time() - ac.last_echo_sent) > 5:

            ac.echo(f"Hi there ~ {{ }} ~ ~{n_loops}")
    time.sleep(0.01)
