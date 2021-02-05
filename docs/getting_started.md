# Getting Started

* Build docker image (cd code/; make docker-image)
* Run Linux-side unit tests (cd code/; make test-unity)
* Build firmware (make all and then make flash)

To add new code files, separate out your logic from your hardware as
much as possible.  This means you probably want to add:

* src/foo.c -- foo's logic
* src/foo_hw.c -- foo's hardware glue
* src/tests/test_foo.c -- code to test the logic code in foo.c
* Add foo.c and foo_hw.c to CFILES in code/Makefile

TODO Finish this document