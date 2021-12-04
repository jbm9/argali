#!/usr/bin/env python3

# Snoops the argali connection

import os
import sys
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from argali_tether.argali_target import ArgaliTarget
from argali_tether.packets import EchoRequestPacket

# Get the default argali argument parser
parser = ArgaliTarget.argparser()
args = parser.parse_args()
tgt = ArgaliTarget.from_args(args)

def logline(f):
    payload = f.payload
    decoded = f'  Log: {payload.decode("iso8859-1")}'
    l = decoded
    print(l)

tgt.register_logline_cb(logline)

tgt.dump_serial_inbound = True

while True:
    if not tgt.pending_input():
        ep = EchoRequestPacket(content=b"hi mom")
        tgt.queue_packet(ep)
        tgt.last_echo_sent = time.time()
        tgt.pending_echo = True
        print("Sending")
    else:
        print(tgt.pending_echo, tgt.pending_dac, tgt.pending_adc_bytes)
        if time.time() - tgt.last_echo_sent > 3:
            tgt.pending_echo = False # janktown
            tgt.last_echo_sent = None
    tgt.poll()
    time.sleep(0.01)
