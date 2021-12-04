#!/usr/bin/env python3

import unittest

from context import packets

class TestEchoPackets(unittest.TestCase):
    def test_foo(self):
        
        echo_req = packets.EchoRequestPacket(content=b"hi mom")

        expected_payload = b'EQ\x00\x06hi mom'
        
        self.assertEqual(expected_payload, echo_req.pack())

        echo_req_parse = packets.EchoRequestPacket.unpack(expected_payload)
        self.assertEqual(echo_req.content, echo_req_parse.content)

if __name__ == '__main__':
    unittest.main()
