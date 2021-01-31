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

docker-test:
	$(DOCKER_RUN) make host-test


####################

host-test-build:
	gcc -g src/tamo_state.c src/test_tamo_state.c external/Unity/build/libunity.a -I external/Unity/src -o bin/test_tamo_state

host-test: host-test-build
	./bin/test_tamo_state

host-test-gdb: host-test-build
	gdb ./bin/test_tamo_state
