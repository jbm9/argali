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

parser.add_argument("--start", help="start the DAC", action="store_true")
parser.add_argument("--stop", help="stop the DAC", action="store_true")
parser.add_argument("--configure", help="Configure the DAC", action="store_true")

DACConfigPacket.add_arguments(parser)


args = parser.parse_args()
tgt = ArgaliTarget.from_args(args)

def logline(f):
    payload = f.payload
    decoded = f'  Log: {payload.decode("iso8859-1")}'
    l = decoded
    print(l)

tgt.register_logline_cb(logline)

was_valid = False

if args.stop:
    pkt = DACStopPacket()
    was_valid = True
    tgt.queue_packet(pkt)
    tgt.poll()

if args.configure:
    pkt = DACConfigPacket.from_args(args)
    was_valid = True    
    tgt.queue_packet(pkt)
    tgt.poll()

if args.start:
    pkt = DACStartPacket()
    was_valid = True    
    tgt.queue_packet(pkt)
    tgt.poll()
    
if not was_valid:
    print("Need one of --start,--stop, or --configure (with its args)")
    sys.exit(1)


tgt.queue_packet(pkt)
tgt.poll()
time.sleep(1)
