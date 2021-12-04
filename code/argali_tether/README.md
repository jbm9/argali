# ArgaliTether

This is a python library to communicate with a tethered Argali
implementation.  It's intended to be used for end-of-line (EOL)
testing of embedded devices.

It comes in two parts:
* A local flavor of HDLC serial port framing
* A baseline set of tools for controlling an EOL unit

It does not currently include any provisions for flashing the device
under test: that's something that you will need to do yourself.  Once
Argali has a bootloader, we could support that through this as well.
