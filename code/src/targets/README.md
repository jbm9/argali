# Target platform directory

This directory contains all the files that are specific to a given
PCB/target.  To add a new board, it's probably easiest to copy an
existing target's directory, then go through and edit every file in
there.

This does result in a bit of copypasta to maintain, but that's often
better than the `#ifdef` tarpit you get when you try to support
multiple different PCBs in the same file.  That said, this is part of
why you probably want to be very fastidious about factoring your code
so that nothing but actual hardware-specific code lives in this
directory.  This minimizes the surface area you're copying around
every time.


## nucleo_f767zi

This is the target used for the initial prototype development, the ST
Nucleo F767ZI devboard.  All functionality is supported on this board.

## nucleo_f413zh

This is the first scale-down target, used to help cost-reduce the
TamoDevBoard project.  All functionality is supported here, but
advanced AI functionality may be limited due to the smaller processer
and limited memory footprint.

Also it's less fancy, but our project doesn't use said fanciness.

