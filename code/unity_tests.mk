# This is based very heavily on the Makefile that is very nicely
# walked through at http://www.throwtheswitch.org/build/make .
# However, there were a bunch of things in there that complicated it
# unnecessarily for our very tightly-controlled build environment.
# This has been streamlined, and some of the strong opinions baked
# into the original have been made much more reasonable (i.e. they now
# match my own strong opinions).

# This will build unit tests for everything named src/test_*.c, and
# will automagically link against the matching C file.  So, if you
# have src/test_foo.c, it will compile both it and foo.c together into
# an executable named build/test_foo.testrunner, then run it and
# capture the output in build/results/test_foo.txt.

CLEANUP = rm -f
MKDIR = mkdir -p
TARGET_EXTENSION = testrunner

.PHONY: clean-unity
.PHONY: test-unity

PATHU = external/Unity/src/
PATHS = src/
PATHT = src/tests/
PATHB = build_test/
PATHD = $(PATHB)/depends/
PATHO = $(PATHB)/objs/
PATHR = $(PATHB)/results/

BUILD_PATHS = $(PATHB) $(PATHD) $(PATHO) $(PATHR)

SRCT = $(wildcard $(PATHT)test_*.c)

COMPILE=gcc -c
LINK=gcc
DEPEND=gcc -MM -MG -MF
CFLAGS=-I. -I$(PATHU) -I$(PATHS) -g -O0 -DTEST

RESULTS = $(patsubst $(PATHT)test_%.c,$(PATHR)test_%.txt,$(SRCT) )

PASSED = `grep -s PASS $(PATHR)*.txt`
FAIL = `grep -s FAIL $(PATHR)*.txt`
IGNORE = `grep -s IGNORE $(PATHR)*.txt`

UNITY_O = $(PATHU)/unity.o

# This is a nasty hack needed because libopencm3 wants to use CC for
# its own ends.
$(PATHU)unity.o: $(PATHU)unity.c
	$(COMPILE) $(CFLAGS) $< -o $@

test-unity:  $(PATHU)unity.o $(BUILD_PATHS) $(RESULTS)
	@echo "-----------------------\nIGNORES:\n-----------------------"
	@echo "$(IGNORE)"
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@echo "$(FAIL)"
	@echo "-----------------------\nPASSED:\n-----------------------"
	@echo "$(PASSED)"
	@echo "\nDONE"

$(PATHR)%.txt: $(PATHB)%.$(TARGET_EXTENSION)
	-./$< > $@ 2>&1

$(PATHB)test_%.$(TARGET_EXTENSION):  $(PATHO)test_%.o $(PATHO)%.o $(PATHU)unity.o #$(PATHD)test_%.d
	$(LINK) -o $@ $^

$(PATHO)%.o:: $(PATHT)%.c
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATHO)%.o:: $(PATHS)%.c
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATHO)%.o:: $(PATHU)%.c $(PATHU)%.h
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATHD)%.d:: $(PATHT)%.c
	$(DEPEND) $@ $<

$(PATHB):
	$(MKDIR) $(PATHB)

$(PATHD):
	$(MKDIR) $(PATHD)

$(PATHO):
	$(MKDIR) $(PATHO)

$(PATHR):
	$(MKDIR) $(PATHR)

clean-unity:
	$(CLEANUP) $(PATHO)*.o
	$(CLEANUP) $(PATHB)*.$(TARGET_EXTENSION)
	$(CLEANUP) $(PATHR)*.txt

.PRECIOUS: $(PATHB)test_%.$(TARGET_EXTENSION)
.PRECIOUS: $(PATHD)%.d
.PRECIOUS: $(PATHO)%.o
.PRECIOUS: $(PATHR)%.txt
