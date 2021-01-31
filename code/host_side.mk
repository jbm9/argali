#
# A Makefile snippet for the stuff that runs on the host itself, and
# isn't just compiling the code.  E.g. clang-format, and eventually
# the unit testing.  Probably building the docker images as well.
#
# TODO: Implement docker building
# TODO: Implement clang-format
# TODO: Implement unit testing on the host side
#


DOCKER_RUN=docker run --privileged -v $(shell pwd):/code -u $(shell id -u):$(shell id -g) -it argali

indent:
	echo clang-format -i $(PATHS)

docker-image:
	docker build -t argali -f Dockerfile .

docker-build:
	$(DOCKER_RUN) make all

docker-flash:
	$(DOCKER_RUN) make flash
