#!/usr/bin/env python3

import serial

import struct

from enum import Enum

class FrameAddress(Enum):
    '''Allowable addresses for packets we can send/receive'''
    DEVICE = b'd'
    DUT = b't'
    CONTROL = b'C'

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
        fmt = f"!cBH{l}s"

        body = struct.pack(fmt, 
                           address.value, control, 
                           l, escaped_payload)

        # Checksum includes all fields except the frame flags and
        # FCS itself, so we need to prepare its input separately.
        cksum = Framer.fcs(body)
        
        result = struct.pack(f'!c{l+4}sHc', cls.FLAG, body, cksum, cls.FLAG)
        
        return result

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
                
    def rx_byte(self, b):
        is_escape = (b == ord(Framer.ESCAPE))
        is_flag = (b == ord(Framer.FLAG))
        
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
            self.state = RXState.IN_BOD

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


last_bytes = []

def print_frame(f):
    print(f'Frame: {f.address}/{f.control}: {f.payload}')
    last_bytes = []
    
rx = SerialRX(print_frame)

s = serial.Serial("/dev/ttyACM0", 115200)

while True:
    buf = s.read(1)
    last_bytes.append(buf[0])
    #print(f'<< {buf[0]:02x}')
    try:
        rx.rx_byte(buf[0])
    except Exception as e:
        print(f'!! Caught exception: {e}')
        rx._reset_state()
        print(f'Bytes up to this: {list(map(chr, last_bytes))}')
        last_bytes = []
