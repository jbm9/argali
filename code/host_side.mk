#
# A Makefile snippet for the stuff that runs on the host itself, and
# isn't just compiling the code.  E.g. clang-format, and eventually
# the unit testing.  Probably building the docker images as well.
#
# TODO: Implement clang-format
#

include unity_tests.mk


DOCKER_RUN=docker run --privileged -v $(shell pwd):/code -u $(shell id -u):$(shell id -g) -it argali

.PHONY: indent
.PHONY: docker-image
.PHONY: docker-build docker-flash
.PHONY: docker-test

indent:
	echo clang-format -i $(PATHS)

docker-image:
	docker build -t argali -f Dockerfile .

docker-build:
	$(DOCKER_RUN) make all

docker-flash:
	$(DOCKER_RUN) make flash

docker-test:
	$(DOCKER_RUN) make test-unity
