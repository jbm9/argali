#
# A Makefile snippet for the protobufs stuff.
#
# Note that this expects you to have included the
# nanopb/extra/nanopb.mk file before you include this one.
ifndef NANOPB_DIR
$(error NANOPB_DIR is not set, ensure that our parent makefile includes extra/nanopb.mk from the nanopb checkout.)
endif


CFLAGS += "-I$(NANOPB_DIR)"

# Add in the core encoder/decoder and common routines
CFILES += $(NANOPB_CORE)

# Makefile voodoo to automatically include all .proto files
PBCFILES = $(patsubst %.proto,%.pb.c,$(wildcard src/packets/logline.proto))
CFILES += $(PBCFILES)
#$(info PROTOS $(wildcard src/packets/*.proto) // $(PBCFILES))
# We implicitly assume the .h files are made for us, which is dirty but works
