#!/bin/sh

echo "Running python unit tests and building HTML coverage"

# This should be running in /code, so we need to hop into the directory
cd argali_tether

coverage run tests/test_*.py
exitval=$?  # Stash this

if [ "x0" != "x$exitval" ];
then
    echo
    echo
    echo '****************************'    
    echo UNIT TESTS FAILED, SEE ABOVE
    echo '****************************'    
    echo
    echo
fi

echo "Building HTML output..."
coverage html --omit '/usr/*'

# And exit with the exit value we got from the test runner
exit $exitval
