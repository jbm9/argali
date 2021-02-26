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