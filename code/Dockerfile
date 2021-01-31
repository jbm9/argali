######################################################################
# A docker image for STM32 development with OpenOCD
#
# This creates a docker image that can be used to build and flash a
# Makefile-based STM32 project (such as those using libopencm3).  It
# does not include any of the usual helper libraries (eg HAL, CMSIS,
# libopencm3, etc), so you'll need to bring your own hardware access
# layer if you want to use one of those.  If you don't, and are crazy
# enough to run bare bare-metal on a Cortex M, I salute you.
#
# This also exports the ports needed to interact with OpenOCD or gdb,
# so you can run gdb-multiarch on your host computer to get to the
# devboard.
#
# Please note that this is originally part of the argali project,
# which ships with a starter firmware package, as well as Makefiles
# that simplify both building this image and using it.
#
# In the argali demo, you can cd into the argali/code directory and
# run 'make docker-image' to build the image.
#
# To flash your code to a devboard, it's just 'make docker-flash'
#

######################################################################
# If you want to do this the hard way, build the image:
#
# docker build -t argali -f Dockerfile .
#
# and then run commands via:
#
# docker run --privileged -v $(pwd):/code -u $(id -u):$(id -g) -it argali  make


######################################################################
# A NOTE ON SECURITY IN MULTI-USER ENVIRONMENTS:
#
# If you're just running this on your laptop, you probably don't care
# about the following.
#
# Note that this runs with privileged to get access to the ST-Link
# programmer, and runs OpenOCD setuid!  If you're running this on your
# personal laptop, that's probably not a real big idea.  But if you're
# deploying this in a shared environment, that's
# probably... suboptimal.  OpenOCD isn't intended to be hardened in
# that kind of environment, and this may open a meaningful security
# hole on multi-user systems.  But, if all the users on a machine have
# superuser access (E.g. a default ubuntu install with a single user),
# this is a pretty thoroughly moot point.
#
# It would be nice to figure out how to get libusb to allow VID/PID
# access within the docker container without needing this hack.
# There's an alternative that involves passing the whole of /dev into
# the container, which may allow udev rules to change permissions.
# However, the need to pass in /dev is mildly terrifying, so I went
# with this hack.


######################################################################
# The actual Dockerfile follows

FROM ubuntu:20.04

# More like ARGH amirite: avoid annoying tzdata prompts.
ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=Etc/UTC 

RUN apt-get update

# Install our base tooling
RUN apt-get install -y openocd gcc-arm-none-eabi gdb-multiarch make python usbutils git 

# Install clang-format
RUN apt-get install -y clang-format

# Install Doxygen and associated tools
RUN apt-get install -y doxygen texlive graphviz 

# And remove stuff we don't need to keep in our already-large image
RUN apt-get purge -y --auto-remove

# Expose the gdb and openocd ports so we can use it from other places
EXPOSE 3333 4444 6666

# Ensure we have a code directory

RUN mkdir /code
WORKDIR /code

############################################################
#
# SECURITY GOTCHA
#
# We want to run openocd as a non-privileged user, but we can't use
# udev rules within docker containers.  So, we have to setuid the
# openocd binary that gets installed.  Please see the long paragraph
# above for more about this.

RUN chmod u+s /usr/bin/openocd