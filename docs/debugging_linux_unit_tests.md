# Debugging Linux-side Unit Tests {#debugging_unit_test_walkthrough}

Argali supports writing unit tests which don't run on your embedded
hardware.  Instead, these tests can be run within the Linux docker
image, where your programs can be bigger than a megabyte.  This also
allows unit testing to be run inside of CI without needing to have a
devboard tethered to your Travis or Jenkins machine.  You can
therefore run a lot of tests in your cloud infrastructure, which is a
lot easier to set up.

While writing these unit tests, it is implausible, but possible, that
you may discover bugs in your code.  In the unlikely event that you
do, you may wish to re-run the test under `gdb` to step through the
function in question.

For the purposes of this demonstration, I have intentionally broken a
unit test:

```
0] jbm@compressability:~/projects/argali/code$ make test-unity
gcc -c -I. -Iexternal/Unity/src/ -Isrc/ -g -O0 -DTEST external/Unity/src//unity.c -o external/Unity/src//unity.o
gcc -o build_test/test_tamo_state.testrunner build_test//objs/test_tamo_state.o build_test//objs/tamo_state.o external/Unity/src/unity.o
./build_test/test_tamo_state.testrunner > build_test//results/test_tamo_state.txt 2>&1
make: [unity_tests.mk:62: build_test//results/test_tamo_state.txt] Error 1 (ignored)
-----------------------
IGNORES:
-----------------------

-----------------------
FAILURES:
-----------------------
src/tests/test_tamo_state.c:289:test_time_travel_to_happy:FAIL: Expected 1 Was 2
FAIL
-----------------------
PASSED:
-----------------------
src/tests/test_tamo_state.c:298:test_initial_state:PASS
src/tests/test_tamo_state.c:299:test_lonely_to_lonely:PASS
src/tests/test_tamo_state.c:300:test_lonely_to_happy:PASS
src/tests/test_tamo_state.c:301:test_happy_to_lonely:PASS
src/tests/test_tamo_state.c:302:test_happy_to_happy:PASS
src/tests/test_tamo_state.c:303:test_happy_to_bored:PASS
src/tests/test_tamo_state.c:304:test_happy_to_bored_revisit:PASS
src/tests/test_tamo_state.c:305:test_bored_to_lonely_direct:PASS
src/tests/test_tamo_state.c:308:test_unknown_to_lonely:PASS
src/tests/test_tamo_state.c:309:test_unknown_to_happy:PASS
src/tests/test_tamo_state.c:310:test_invalid_to_lonely:PASS
src/tests/test_tamo_state.c:311:test_time_travel_to_lonely:PASS

DONE
rm external/Unity/src/unity.o
```

We see that there's a problem on `src/tests/test_tamo_state.c:289`:

```
TEST_ASSERT_TAMO_EDGE(t_warp, USER_IS_PRESENT, CHANGE_YES, t_warp, TAMO_LONELY);
```

But that's a gnarly compound assertion.  Which part is breaking?
We'll want to use gdb to find out.  Let's pop into a docker shell
where we can re-run commands manually.

We begin by making sure the problem occurs even when we clean up all
the compilation outputs:

```
0] jbm@compressability:~/projects/argali/code$ ./docker_shell.sh 
groups: cannot find name for group ID 1000
0] argali-docker@f00019a60a05:/code$ make clean-unity
rm -f build_test//objs/*.o
rm -f build_test/*.testrunner
rm -f build_test//results/*.txt
0] argali-docker@f00019a60a05:/code$ make test-unity
gcc -c -I. -Iexternal/Unity/src/ -Isrc/ -g -O0 -DTEST external/Unity/src//unity.c -o external/Unity/src//unity.o
gcc -c -I. -Iexternal/Unity/src/ -Isrc/ -g -O0 -DTEST src/tests/test_tamo_state.c -o build_test//objs/test_tamo_state.o
gcc -c -I. -Iexternal/Unity/src/ -Isrc/ -g -O0 -DTEST src/tamo_state.c -o build_test//objs/tamo_state.o
gcc -o build_test/test_tamo_state.testrunner build_test//objs/test_tamo_state.o build_test//objs/tamo_state.o external/Unity/src/unity.o
./build_test/test_tamo_state.testrunner > build_test//results/test_tamo_state.txt 2>&1
make: [unity_tests.mk:62: build_test//results/test_tamo_state.txt] Error 1 (ignored)
-----------------------
IGNORES:
-----------------------

-----------------------
FAILURES:
-----------------------
src/tests/test_tamo_state.c:289:test_time_travel_to_happy:FAIL: Expected 1 Was 2
FAIL
-----------------------
PASSED:
-----------------------
src/tests/test_tamo_state.c:298:test_initial_state:PASS
src/tests/test_tamo_state.c:299:test_lonely_to_lonely:PASS
src/tests/test_tamo_state.c:300:test_lonely_to_happy:PASS
src/tests/test_tamo_state.c:301:test_happy_to_lonely:PASS
src/tests/test_tamo_state.c:302:test_happy_to_happy:PASS
src/tests/test_tamo_state.c:303:test_happy_to_bored:PASS
src/tests/test_tamo_state.c:304:test_happy_to_bored_revisit:PASS
src/tests/test_tamo_state.c:305:test_bored_to_lonely_direct:PASS
src/tests/test_tamo_state.c:308:test_unknown_to_lonely:PASS
src/tests/test_tamo_state.c:309:test_unknown_to_happy:PASS
src/tests/test_tamo_state.c:310:test_invalid_to_lonely:PASS
src/tests/test_tamo_state.c:311:test_time_travel_to_lonely:PASS

DONE
rm external/Unity/src/unity.o
0] argali-docker@f00019a60a05:/code$
```

We have confirmed that the problem persists, so now it's time to hop
into the debugger.  The problem is in `test_tamo_state.c`, which is
compiled to a binary of `build_test/test_tamo_state.testrunner`.
We'll launch gdb on that, and set a breakpoint on
`test_tamo_state.c:289`, and then start the program running:


```
0] argali-docker@f00019a60a05:/code$ gdb ./build_test/test_tamo_state.testrunner 
GNU gdb (Ubuntu 9.2-0ubuntu1~20.04) 9.2
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from ./build_test/test_tamo_state.testrunner...
(gdb) break test_tamo_state.c:289
Breakpoint 1 at 0x1de3: file src/tests/test_tamo_state.c, line 289.
(gdb) r
Starting program: /code/build_test/test_tamo_state.testrunner 
src/tests/test_tamo_state.c:298:test_initial_state:PASS
src/tests/test_tamo_state.c:299:test_lonely_to_lonely:PASS
src/tests/test_tamo_state.c:300:test_lonely_to_happy:PASS
src/tests/test_tamo_state.c:301:test_happy_to_lonely:PASS
src/tests/test_tamo_state.c:302:test_happy_to_happy:PASS
src/tests/test_tamo_state.c:303:test_happy_to_bored:PASS
src/tests/test_tamo_state.c:304:test_happy_to_bored_revisit:PASS
src/tests/test_tamo_state.c:305:test_bored_to_lonely_direct:PASS
src/tests/test_tamo_state.c:308:test_unknown_to_lonely:PASS
src/tests/test_tamo_state.c:309:test_unknown_to_happy:PASS
src/tests/test_tamo_state.c:310:test_invalid_to_lonely:PASS
src/tests/test_tamo_state.c:311:test_time_travel_to_lonely:PASS

Breakpoint 1, test_time_travel_to_happy () at src/tests/test_tamo_state.c:289
289	  TEST_ASSERT_TAMO_EDGE(t_warp, USER_IS_PRESENT, CHANGE_YES, t_warp,
```

Now that we've hit the breakpoint, we can debug the problem as usual.
I will actually skip that, because at this point you're just running
`gdb` on a normal Linux binary.  A tutorial on `gdb` is,
unfortunately, well outside the scope of this document.

One thing is worth noting, though: you don't need to exit gdb to
rebuild your binary.  Since you're running the debugger within the
same docker image as you build, you can simply rebuild from the `gdb`
prompt and it should work okay.

So, once you have found the problem, you rebuild the specific test
runner and restart:

```
(gdb) make build_test/test_tamo_state.testrunner 
gcc -c -I. -Iexternal/Unity/src/ -Isrc/ -g -O0 -DTEST src/tests/test_tamo_state.c -o build_test//objs/test_tamo_state.o
gcc -o build_test/test_tamo_state.testrunner build_test//objs/test_tamo_state.o build_test//objs/tamo_state.o external/Unity/src/unity.o
(gdb) r
The program being debugged has been started already.
Start it from the beginning? (y or n) y
`/code/build_test/test_tamo_state.testrunner' has changed; re-reading symbols.
Starting program: /code/build_test/test_tamo_state.testrunner 
warning: Probes-based dynamic linker interface failed.
Reverting to original interface.
src/tests/test_tamo_state.c:298:test_initial_state:PASS
src/tests/test_tamo_state.c:299:test_lonely_to_lonely:PASS
src/tests/test_tamo_state.c:300:test_lonely_to_happy:PASS
src/tests/test_tamo_state.c:301:test_happy_to_lonely:PASS
src/tests/test_tamo_state.c:302:test_happy_to_happy:PASS
src/tests/test_tamo_state.c:303:test_happy_to_bored:PASS
src/tests/test_tamo_state.c:304:test_happy_to_bored_revisit:PASS
src/tests/test_tamo_state.c:305:test_bored_to_lonely_direct:PASS
src/tests/test_tamo_state.c:308:test_unknown_to_lonely:PASS
src/tests/test_tamo_state.c:309:test_unknown_to_happy:PASS
src/tests/test_tamo_state.c:310:test_invalid_to_lonely:PASS
src/tests/test_tamo_state.c:311:test_time_travel_to_lonely:PASS

Breakpoint 1, test_time_travel_to_happy () at src/tests/test_tamo_state.c:289
289	  TEST_ASSERT_TAMO_EDGE(t_warp, USER_IS_PRESENT, CHANGE_YES, t_warp,
(gdb) c
Continuing.
src/tests/test_tamo_state.c:312:test_time_travel_to_happy:PASS

-----------------------
13 Tests 0 Failures 0 Ignored 
OK
[Inferior 1 (process 196) exited normally]
(gdb) 
```

In the transcript above, I undid the error that was introduced in the
unit test code, so the breakpoints didn't move etc.

Once you're done, it's probably worth exiting out and re-running

```
make clean-unity test-unity
```

This should get to a known-good state, which lets you commit with
confidence that you're not going to break the (unit testing) build.
You will also want to go through any manual testing on devboard
hardware to make sure you haven't accidentally broken the firmware in
this process.