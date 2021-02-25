#!/bin/bash
#


# This is a helper script to build OpenOCD from scratch at a
# "known-good" point in the repo's history.  Fixing to a commit allows
# us to make sure it compiles every time, but with the ironic caveat
# that, like the distribution's release, this one may age less than
# gracefully.

CHECKOUT_REPO=git://git.code.sf.net/p/openocd/code
CHECKOUT_PATH=/openocd/
CHECKOUT_HASH=ba0f382137749b78b27ac58238735cc20a6fa847

[ $DEBUG ] && echo "Checking out OpenOCD from: ${CHECKOUT_REPO}"
[ $DEBUG ] && echo "         Target directory: ${CHECKOUT_PATH}"
[ $DEBUG ] && echo "        Target git commit: ${CHECKOUT_HASH}"



############################################################
# Check out the code

[ $DEBUG ] && echo "Beginning git clone... (sourceforge is slow sometimes) $(date)"
git clone ${CHECKOUT_REPO} ${CHECKOUT_PATH}

if [ "x$?" != "x0" ];
then
    echo "Some error occurred in the git clone of OpenOCD.  Please see above."
    exit 1
fi
[ $DEBUG ] && echo "Clone complete $(date)"

[ $DEBUG ] && echo "Changing directories to ${CHECKOUT_PATH} $(date)"
cd ${CHECKOUT_PATH}

[ $DEBUG ] && echo "Checking out the known-good revision of OpenOCD... $(date)"
git checkout -b argali_no_detached_head ${CHECKOUT_HASH}
[ $DEBUG ] && echo "Checking out the known-good revision of OpenOCD... $(date)"



############################################################
# Build the code

[ $DEBUG ] && echo "Beginning the build process...  $(date)"

[ $DEBUG ] && echo "    Running autotools bootstrap...  $(date)"
./bootstrap

[ $DEBUG ] && echo "    Running configure...  $(date)"
./configure

[ $DEBUG ] && echo "    Running make...  $(date)"
make

[ $DEBUG ] && echo "Build complete.  $(date)"


############################################################
# Install the new openocd
[ $DEBUG ] && echo "Installing the package (via make install)... $(date)"
make install

[ $DEBUG ] && echo "New OpenOCD built and installed.  $(date)"


# The above [ $DEBUG ] output test causes $? to be non-zero if you
# aren't running in debug mode, which causes the docker build to error
# out.  So, we explicitly exit cleanly here.

exit 0
