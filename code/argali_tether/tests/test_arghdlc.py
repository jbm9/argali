#!/usr/bin/env python3

import unittest

from context import arghdlc

class TestFramer(unittest.TestCase):
    def test_framing(self):
        # This is a direct parallel of the same test in the firmware

        cases = [ (b"", 0, b"~d\x00\x00\x00\xe8)~", 8, 0 ),
                  (b"~asdf~foo}{}", 12, b"~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 24, 0)
                  ]

        for i, vals in enumerate(cases):
            buf,buflen,expected,expected_len,expected_pkt_interrupts = vals            
            got = arghdlc.Framer.frame(buf, arghdlc.FrameAddress.DEVICE, 0)
            self.assertEqual(expected, got, f'Case {i}')
            self.assertEqual(expected_len, len(got), f'Case {i}')
        

class TestDeframer(unittest.TestCase):
    def test_foo(self):

        cases = [
            (b"~asdf~foo}{}", 12, b"~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 24, 0 ),
            (b"", 0, b"~d\x00\x00\x00\xe8)~", 8, 0),
            # // Test a case where we get an unescaped ~ in body: should reset state
            ( b"~asdf~foo}{}", 12, b"~d\x00\x00\x10}~a~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 32, 1 ),
            ]

        got_frame = None
        def _cb(f):
            nonlocal got_frame
            got_frame = f

        n_interrupted = 0
        def _cb_interrupted(bs):
            nonlocal n_interrupted
            n_interrupted += 1

        deframer = arghdlc.Deframer(_cb)
        deframer.register_interrupted_packet_cb(_cb_interrupted)
        
        for i, vals in enumerate(cases):
            buf,buflen,expected,expected_len,expected_pkt_interrupts = vals

            got_frame = None
            n_interrupted = 0

            for b in expected:
                deframer.rx_byte(b)

            self.assertIsNotNone(got_frame, f'Case {i}')
            self.assertEqual(expected_pkt_interrupts, n_interrupted, f'Case {i}')  # TODO actually support these
            
            self.assertEqual(buf, got_frame.payload, f'Case {i}')


    def test_roundtrip(self):
        loglevel = 10
        msg = "hi mom"
        
        framed = arghdlc.Framer.frame(msg.encode('ISO8859-1'))

        got_frame = None
        got_content = None
        def _cb(f):
            nonlocal got_frame
            nonlocal got_content
            got_frame = f
            got_content = f.payload.decode("ISO8859-1")


        deframer = arghdlc.Deframer(_cb)

        deframer.rx(framed)

        self.assertEqual(msg, got_content)

        

if __name__ == '__main__':
    unittest.main()
