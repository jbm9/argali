#!/usr/bin/env python3

# Snoops the argali connection

import os
import sys
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from argali_tether.argali_target import ArgaliTarget
from argali_tether.packets import *

# Get the default argali argument parser
parser = ArgaliTarget.argparser()

parser.add_argument("--request", help="Request readings from the ADC", action="store_true")
parser.add_argument("-q", help="Don't show the offsets, just return a blob of hex", action="store_true")

ADCConfigPacket.add_arguments(parser)

args = parser.parse_args()
tgt = ArgaliTarget.from_args(args)

def logline(f):
    payload = f.payload
    decoded = f'  Log: {payload.decode("iso8859-1")}'
    l = decoded
    print(l)

def adc_cb(buf):
    for i in range(0, len(buf), 16):
        prefix = "" if args.q else f'{i:4d}: '
        print(f'{prefix}{buf[i:i+16].hex()}')
    
    
tgt.register_logline_cb(logline)
tgt.set_adc_cb(adc_cb)

if args.request:
    print(args)
    pkt = ADCConfigPacket.from_args(args)
else:
    print("Need --request (with its args)")
    sys.exit(1)


tgt.pending_adc_bytes = pkt.num_points * pkt.sample_width * len(pkt.channels)
tgt.queue_packet(pkt)

while tgt.pending_adc_bytes > 0:
    tgt.poll()
    time.sleep(0.01)
