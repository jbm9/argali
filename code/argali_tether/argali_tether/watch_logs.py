#!/usr/bin/env python3

# Snoops the argali connection

import os
import sys
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from argali_tether.argali_target import ArgaliTarget

# Get the default argali argument parser
parser = ArgaliTarget.argparser()
parser.add_argument("--timestamp",
                    help="Add local timestamps to each logline",
                    action="store_true")

args = parser.parse_args()
tgt = ArgaliTarget.from_args(args)

def logline(p):
    decoded = f'[{p.loglevel.Name(p.level)}] {p.content}' # .decode("iso8859-1")
    l = decoded
    if args.timestamp:
        l = f'{time.strftime("%c")} {decoded}'
    print(l)

tgt.register_logline_cb(logline)

while True:
    tgt.poll()
    time.sleep(0.01)
