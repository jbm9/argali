#
# A Makefile snippet for the stuff that runs on the host itself, and
# isn't just compiling the code.  E.g. clang-format, and eventually
# the unit testing.  Probably building the docker images as well.
#
# TODO: Implement clang-format
#

# Please note that we do something inside out for unity testing.  This
# requires a bunch of redefinition of CFLAGS etc, which we'd rather
# not have changed during the main firmware build.  Because it breaks
# things.  So instead of including that file here, we instead include
# the main Makefile in unity's Makefile.

DOCKER_RUN_BASE=docker run --privileged -v $(shell pwd):/code -v $(shell cd ..; pwd):/project_base -u $(shell id -u):$(shell id -g)
DOCKER_RUN=$(DOCKER_RUN_BASE) -it argali

OOCD_PORT ?= 4444

# Clump all the variables we need to pass in to docker makes in one place
ARGALIVARS=TARGET=$(TARGET) OOCD_FILE=$(OOCD_FILE) OOCD_INTERFACE=$(OOCD_INTERFACE) V=$(V)

.PHONY: docker-image
.PHONY: docker-build docker-flash
.PHONY: docker-test
.PHONY: docker-openocd-daemon docker-gdbgui
.PHONY: docker-doxygen

docker-image:
	docker build -t argali -f Dockerfile .

docker-build:
	$(DOCKER_RUN)  make all $(ARGALIVARS)

docker-flash:
# TODO You can't currently run this command while the openocd-daemon
# is running, since it has grabbed $(OOCD_PORT) for its container,
# which can't be seen from here.  Probably need to add a network for this?
	$(DOCKER_RUN) make flash $(ARGALIVARS)

docker-test:
# NB: This one doesn't use ARGALIVARS, since those are for building firmware
	$(DOCKER_RUN) make -f unity_tests.mk test-unity

docker-openocd-daemon:
	$(DOCKER_RUN_BASE) --net=host -p 127.0.0.1:$(OOCD_PORT):$(OOCD_PORT) -p 127.0.0.1:3333:3333 -it argali make openocd-daemon $(ARGALIVARS)

docker-gdbgui:
# TODO Check that the openocd port is open
	$(DOCKER_RUN_BASE) --net=host -p 127.0.0.1:5000:5000 -e HOME=/code -it argali gdbgui -g gdb-multiarch --args /code/$(PROJECT).elf


docker-doxygen:
	$(DOCKER_RUN) ./build_doxygen.sh
