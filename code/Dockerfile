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
#
# There may also be some concerns with the use of gdbgui.  This is
# entirely optional, but it is so very, very nice.


######################################################################
# Please note that this Dockerfile is under active development, so its
# actions are in a weird/suboptimal order.  This is to make the builds
# reasonably quick while doing development.  If you weren't aware,
# Docker effectively builds an image at each line, so *appends* are
# cheap.  It's very neat.
#
#
# Anyway, I may leave it like this, as it helps keep things nicely
# contained.  Unfortunately, each section was developed right where it
# is now, so there may be implicit dependencies on prior steps.
# Apologies if you're trying to develop off of the later subsections.


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

RUN apt-get install -y gcc

############################################################
# Install gdbgui, which is pretty neat

RUN apt-get install -y python3-pip
RUN pip3 install gdbgui
EXPOSE 5000

############################################################
# And build a new cmsis-svd, which is a python package that is
# constantly being updated with new chips and the occasional
# bugfix/tweak, but is never re-released.  For five years now.

RUN mkdir /cmsis-svd
RUN git clone https://github.com/posborne/cmsis-svd/ /cmsis-svd/
RUN cd /cmsis-svd/python; pip3 install .

# SVD files are the heart of this: they're XML that describes the
# register sets of chips.  This install worked automagically with pip
# on my Linux local install, but didn't in Docker, so let's manually
# copy the data over.

RUN cp -r /cmsis-svd/data/ /usr/local/lib/python3.8/dist-packages/cmsis_svd/


############################################################
# Aaand now we build an unreleased OpenOCD, because the one in the
# distribution is pretty old, and doesn't support reasonably common
# chips like the F413.  Sheesh.

RUN apt-get install -y libtool pkg-config libusb-1.0-0-dev
COPY build_openocd.sh /tmp/build_openocd.sh
RUN /tmp/build_openocd.sh
RUN chmod u+s /usr/local/bin/openocd




############################################################
# Add some needed latex packages for Doxygen to assemble docs
RUN apt-get update
RUN apt-get install -y texlive-latex-extra

RUN pip3 install nanopb
# Make the annoying "unknown group" message go away.
RUN addgroup --gid 1000 argali

RUN apt-get install -y protobuf-compiler

RUN pip3 install pyserial

RUN pip3 install coverage
# TODO Make these both just use the requirements.txt
