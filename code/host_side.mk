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

# If you want a custom docker image name, you can change it in
# Makefile before getting here, and we will respect it.
DOCKER_IMAGE ?= $(shell cat .docker_image_name)

DOCKER_RUN_BASE=docker run --privileged -v $(shell pwd):/code -v $(shell cd ..; pwd):/project_base -u $(shell id -u):$(shell id -g)
DOCKER_RUN=$(DOCKER_RUN_BASE) -it $(DOCKER_IMAGE)

OOCD_PORT ?= 4444

# Clump all the variables we need to pass in to docker makes in one place
ARGALIVARS=TARGET=$(TARGET) OOCD_FILE=$(OOCD_FILE) OOCD_INTERFACE=$(OOCD_INTERFACE) V=$(V)

.PHONY: docker-image-latest docker-image
.PHONY: docker-build docker-flash
.PHONY: docker-test
.PHONY: openocd-daemon docker-openocd-daemon docker-gdbgui
.PHONY: docker-doxygen
.PHONY: python-protobufs python-unittests

.argali_setup: docker-image
	git submodule init
	git submodule update
	make docker-image
	make docker-build
	touch .argali_setup

docker-image-latest:
	docker build -t $(DOCKER_IMAGE):latest -f Dockerfile .

docker-image: docker-image-latest
	docker tag $(DOCKER_IMAGE):latest $(DOCKER_IMAGE):$(shell cat VERSION)

docker-build:
	$(DOCKER_RUN) make -C libopencm3 all
	$(DOCKER_RUN) make all $(ARGALIVARS)

docker-flash:
# TODO You can't currently run this command while the openocd-daemon
# is running, since it has grabbed $(OOCD_PORT) for its container,
# which can't be seen from here.  Probably need to add a network for this?
	$(DOCKER_RUN) make flash $(ARGALIVARS)

docker-test:
# NB: This one doesn't use ARGALIVARS, since those are for building firmware
	$(DOCKER_RUN) make -f unity_tests.mk test-unity

openocd-daemon:
	$(OOCD) -f $(OOCD_FILE)

docker-openocd-daemon:
	$(DOCKER_RUN_BASE) --net=host -p 127.0.0.1:$(OOCD_PORT):$(OOCD_PORT) -p 127.0.0.1:3333:3333 -it $(DOCKER_IMAGE) make openocd-daemon $(ARGALIVARS)

docker-gdbgui:
# TODO Check that the openocd port is open
	$(DOCKER_RUN_BASE) --net=host -p 127.0.0.1:5000:5000 -e HOME=/code -it $(DOCKER_IMAGE) gdbgui -g gdb-multiarch --args /code/$(PROJECT).elf


docker-doxygen:
	$(DOCKER_RUN) ./build_doxygen.sh

python-protobufs:
	$(DOCKER_RUN) ./argali_tether/build_protobufs.sh

python-unittests:
	$(DOCKER_RUN) ./argali_tether/run_unittests.sh
