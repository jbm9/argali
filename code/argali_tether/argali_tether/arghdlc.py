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
        '''Enframe a payload in Argali HDLC

        This returns a buffer of bytes that can be sent out via a
        write() method.

        On the device side, Argali does not use the address or control
        locations for anything specific, so you may use them for your
        own purposes if needed.  It is recommended to not use them,
        however, and instead lean on your packet encapsulation within
        HDLC.  This will allow you to replace HDLC with network
        packets (etc) as needed later on.
        '''
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
    def fcs(cls, buf: bytes, crc=0xFFFF):
        for x in buf:
            for i in range(8):
                crc= (crc>>1)^0x8408 if ((crc&1)^(x&1)) else crc>>1
                x >>= 1
        crc = crc & 0xffff
        return crc


class DeframerState(Enum):
    IDLE = 1
    WAIT_ADDR = 2
    WAIT_CONTROL = 3
    WAIT_LEN_HI = 4
    WAIT_LEN_LO = 5
    IN_BODY = 6
    ESCAPE_FOUND = 7
    WAIT_CKSUM_HI = 8
    WAIT_CKSUM_LO = 9

class Deframer:
    MAX_PACKET_LEN = 1500

    def __init__(self, cb):
        '''Set up a Deframer state machine

        cb is a callback which takes a Frame instance as an argument
        '''
        self.state = DeframerState.IDLE
        self.cb = cb
        self.interrupted_packet_cb = None

        self._reset_state()

    def register_interrupted_packet_cb(self, ipcb):
        '''Register an interrupted packet callback

        The argument should be a function that can take a single
        argument, which will be the bytes() received up to this point.
        
        This is called to alert you to a packet coming in and
        interrupting your decode.  You will see things like this when
        a target resets before finishing its last packet.

        To de-register the callback, just call this with `None`

        '''
        self.interrupted_packet_cb = ipcb
        
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
            if self.state == DeframerState.IN_BODY:
                # Escapes in body count toward frame length
                self.body_rem -= 1
                # TODO deal with FCS here
            return

        # If we get an unexpected ~ on the line, we assume that a
        # packet has been interrupted and a new one is commencing.
        #
        if not self.saw_escape \
           and self.state not in [DeframerState.IDLE, DeframerState.WAIT_ADDR] \
           and is_flag:
            # Interrupted packet, reset state
            if self.interrupted_packet_cb:
                self.interrupted_packet_cb(self.accumulator)
            self._reset_state()
            self.state = DeframerState.WAIT_ADDR
            return

        ####################
        # All escape handling was done above this line
        self.saw_escape = False

        if self.state == DeframerState.IDLE:
            if not is_flag:
                # Waiting for a flag; let the connection idle
                return
            self.state = DeframerState.WAIT_ADDR
            return

        if self.state == DeframerState.WAIT_ADDR:
            if is_flag:
                # We allow the channel to idle at ~~~~~~
                return
            self.cur_addr = b
            self.state = DeframerState.WAIT_CONTROL
            return

        if self.state == DeframerState.WAIT_CONTROL:
            self.cur_control = b
            self.state = DeframerState.WAIT_LEN_HI
            return

        # Get our lengths
        if self.state == DeframerState.WAIT_LEN_HI:
            self.cur_len = b << 8
            self.state = DeframerState.WAIT_LEN_LO
            return

        if self.state == DeframerState.WAIT_LEN_LO:
            self.cur_len += b
            self.body_rem = self.cur_len

            if self.cur_len > Deframer.MAX_PACKET_LEN:
                self.state = DeframerState.IDLE
                raise ValueError(f'Packet is too long: {self.cur_len} > {Deframer.MAX_PACKET_LEN}')

            self.state = DeframerState.IN_BODY
            return

        # Wait for the body and possibly its end
        if self.state == DeframerState.IN_BODY and self.body_rem > 0:
            self.body_rem -= 1
            self.accumulator.append(b)
            return

        if self.state == DeframerState.IN_BODY and self.body_rem == 0:
            self.state = DeframerState.WAIT_CKSUM_HI
            # fallthrough
            
        if self.state == DeframerState.WAIT_CKSUM_HI:
            self.cur_cksum = b << 8
            self.state = DeframerState.WAIT_CKSUM_LO
            return

        if self.state == DeframerState.WAIT_CKSUM_LO:
            self.cur_cksum += b
            buf = bytes(self.accumulator)
            # TODO Check checksum
            f = Frame(self.cur_addr, self.cur_control, buf)
            self.cb(f)
            self.state = DeframerState.IDLE
            self._reset_state()

    def rx(self, buf):
        for b in buf:
            self.rx_byte(b)
