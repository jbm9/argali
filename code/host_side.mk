#
# A Makefile snippet for the stuff that runs on the host itself, and
# isn't just compiling the code.  E.g. clang-format, and eventually
# the unit testing.  Probably building the docker images as well.
#
# TODO: Implement docker building
# TODO: Implement clang-format
# TODO: Implement unit testing on the host side
#
indent:
	echo clang-format -i $(PATHS)

