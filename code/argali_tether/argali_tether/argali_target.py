import argparse
from typing import List, Tuple
import time
import sys


import serial
import serial.tools.list_ports

from .arghdlc import Framer, Deframer

class ArgaliTarget:
    '''A tethered Argali device, intended for your EOL station

    This is a library for communicating with your end-of-ilne (EOL)
    station, not the device under test (DUT).  Your DUT is probably a
    separate Argali device, which you could also talk to via this
    protocol if you desired, but EOL testing is usually driven by
    simulating button presses and so on.
    '''
    def __init__(self, s):
        self.s = s
        self.rx = Deframer(self.frame_cb)

        self.last_bytes = []  # Accumulator for bytes, used do debug bad frames

        self.pending_frames = []  # A list of frames to send

        self.dump_serial_inbound = False

        self.logline_cb = None  # Callback to call when we get a log line
        
        self.adc_cb = None
        self.adc_buf = bytes()
        
        self.pending_echo = False
        self.pending_dac = False
        self.pending_adc_bytes = 0

        self.last_echo_sent = None

    @classmethod
    def serial(cls, *args, **kwargs):
        '''Create an ArgaliTarget pointed at a serial port

        This takes all the same args as serial.Serial(), so you
        probably want something like

        ArgaliTarget.serial("/dev/ttyACM0")

        '''
        s = serial.Serial(*args, **kwargs)
        return cls(s)

    @classmethod
    def list_ports(cls) -> List[Tuple[str,str]]:
        '''List available serial ports on the system, with serial numbers

        This returns a list of tuples of the form

        ("/dev/ttyACM0","066DFF575051717867043734"),

        where the serial port itself is "/dev/ttyACM0" and its serial
        number is 066DFF575051717867043734.

        '''
        result = []
        for port in serial.tools.list_ports.comports():
            result.append((port.device, port.serial_number))
        return result

    @classmethod
    def serial_number(cls, serial_number: str, *args, **kwargs):
        '''Open a serial port based on its serial number'''
        for port in serial.tools.list_ports.comports():
            if serial_number == port.serial_number:
                return cls.serial(port.device, *args, **kwargs)
            
        # Couldn't find the serial port; try using list_ports() to
        # double-check that your port is connected.
        
        raise KeyError(f'Could not find a serial port with serial number "{serial_number}"')


    @classmethod
    def argparser(cls):
        '''Get a baseline argument parser that ArgaliTarget can work with.

        You can add your own items to this, though we make no
        guarantees about namespaces.  That is: later releases of
        Argali may add arguments with the same name as yours.
        '''
        parser = argparse.ArgumentParser()
        parser.add_argument("--port", help="Path to serial port")
        parser.add_argument("--port-serial-no", help="Serial number of port to use")
        parser.add_argument("--list-ports", help="List ports", action="store_true", default=False)
        parser.add_argument("--baud",
                            help="Baud rate of serial port (default: 115200)", type=int, default=115200)
        parser.add_argument("--timeout", help="Timeout for serial reads (default 1s, -1 for None)", type=float, default=1.0)

        return parser

    @classmethod
    def from_args(cls, args):
        '''Build an ArgaliTarget from args, or list ports and exit

        This is not the cleanest of methods, but it is relatively nice
        to use.  It wraps up all the details of handling argali's
        default command line arguments, which means that it might
        actually cleanly exit your program on you.

        You pass in the args value:
        ```
        parser = ArgaliTarget.argparser()
        # Add your own arguments here if you want

        args = parser.parse_args()
        tgt = ArgaliTarget.from_args(args)
        ```

        This can raise exceptions if the targeted port isn't found.
        '''

        def _list_ports():
            print("Available serial ports:")
            for devname, serial_number in ArgaliTarget.list_ports():
                print(f'{devname}  --  {serial_number}')

        if args.list_ports:
            _list_ports()
            sys.exit(0)  # TODO should this be a raise()?

        if args.port is None and args.port_serial_no is None:
            raise ValueError('Need either a port name or a serial number to connect to.  --list-ports to find them')

        if args.port_serial_no:
            try:
                return ArgaliTarget.serial_number(args.port_serial_no, baudrate=args.baud)
            except KeyError as e:
                print(e)
                print()
                _list_ports()
                raise

        timeout = args.timeout
        if -1 == timeout:
            timeout = None
        return ArgaliTarget.serial(port=args.port, baudrate=args.baud, timeout=timeout)


    def register_logline_cb(self, cb):
        '''Set the logline callback

        This is called with a full Frame any time a log line comes in.
        You probably want to register something like the following:

        ```
        def print_logline(f):
            print(f.decode("iso8859-1"))
        ```

        But you are free to do whatever you wish here.
        '''
        self.logline_cb = cb
        
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
        '''Poll for events and handle them.
        '''
        buf = self.s.read(10)
        if buf and self.dump_serial_inbound:
            print(f'<< {buf}')
        self.last_bytes.extend(buf)
        try:
            self.rx.rx(buf)
        except Exception as e:
            print(f'Exception {e}: bytes={self.last_bytes}')
            raise
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

    def queue_packet(self, p):
        self.enqueue(Framer.frame(p.pack()))

    def enqueue(self, f):
        #print(f'Enqueueing frame: {f}')
        self.pending_frames.append(f)

    def frame_cb(self, f):
        self.last_bytes = []

        c = f.payload[0] if len(f.payload) else ""
        # print(f'     Got frame: {f.address}/{f.control}: {len(f.payload)} {c}')

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
        if len(f.payload) < 3:
            print(f'Unknown payload: short: {f.payload}')
        else:
            print(f'Unknown payload: {f.payload[0]}/{f.payload[1]} {f.payload[2:].decode("iso8859-1")}')

    def _logline(self, f):
        if None == self.logline_cb:
            print(f'                                         Logline({f.control}): {f.payload.decode("iso8859-1")}')
        else:
            self.logline_cb(f)

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
        # print(f'Got DAC reply')
        self.pending_dac = False

    def set_adc_cb(self, cb):
        self.adc_cb = cb

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
            self.adc_buf += payload[2:]
            self.pending_adc_bytes -= (len(payload)-2)
            # print(f'Got ADC Data: ({self.pending_adc_bytes} after this)')

            if self.pending_adc_bytes == 0:
                if self.adc_cb:
                    self.adc_cb(self.adc_buf)
                else:
                    for i in range(0, len(self.adc_buf), 16):
                        print(f'{i:4d}: {self.adc_buf[i:i+16].hex()}')
                

    def idle(self, n):
        self.tx(b'~' * n)

    def bogon(self):
        self.enqueue(Framer.frame(b'xxfoo'))

