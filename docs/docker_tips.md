# Argali Docker Tips

This document contains a bit of a cookbook for customizing the Argali
docker container.

Argali uses docker for all its build infrastructure.  This makes it
very easy to bring up a new developer's machine with an environment
that is identical to the one used by everyone else.  It also provides
a handy way to fork off and build out your own custom local build
infrastructure.

## Getting started with docker

The [docker tutorial](https://docs.docker.com/get-started/) is aimed
at application developers, but the first bits are relatively general
and may be of help to you.

A more embedded-focused tutorial is Matt Chernosky's blog post,
[Simple embedded build environments with
Docker](http://www.electronvector.com/blog/simple-embedded-build-environments-with-docker).
Note that he has windows instructions in there, which Argali may or
may not support at this time (I don't have a Windows dev environment,
so I can't test it).

## Building the docker image

`make docker-image` will build the latest docker image from the
`Dockerfile`.  It will tag it as `:latest`, and will also tag the
current `VERSION`.  If you won't want to disturb the versioned image,
you can use `make docker-image-latest`.

## Renaming the image to make local modifications

To rename your docker image, you can update `.docker_image_name`.

This is highly recommended if you want to fork off a local Docker
setup that you don't intend to merge upstream.

## Updating the docker image version: `VERSION`

The `VERSION` file is used to version the docker image.  It is Best
Practices to update the version every time you update the
`Dockerfile`.  That said, there is not a good way to communicate image
version dependencies, so this is more useful as an implicit
communication channel, and can be used to checkpoint meaningful points
in the history of the Dockerfile.

## Adding new stuff to your docker image 

As mentioned previously, it is best practice to bump `VERSION` every
time you update the `Dockerfile`.

In general, you will want to **append** to the Dockerfile while doing
development work, then periodically go through and clean it up.  This
is because docker is quite clever about how docker images are built,
so adding a line at the end for `RUN mkdir /data_cache` will result in
a very overlay on top of the last revision of the docker image.

Once you are happy with this, you can go back and pull all the various
`apt-get install` packages onto a single line, bump `VERSION`, and
rebuild.  This will give you a clean image with minimal
thumb-twiddling during builds.

## Running a shell in the Docker image

After you add stuff to the docker image, you will want to work with
it.  Running `./docker_shell.sh` in the `code/` directory will give
you a bash shell inside a new copy of the docker container.  The shell
prompt is a standard bash prompt, but at the beginning of the line is
`$?]`, which shows you the exit code of your most recently run
command.  Most people can just ignore this. 

Note that this shell is running with `--privileged` but with your
userid.  This should make it reasonably safe, but it does have access
to devices on your machine.  Treat it more or less like you would a
shell on your local machine.

Also, we do a bit of juggling to get a usable `.bash_history` into
this shell.  You should be able to get command history across
invocations of the docker shell this way, which is surprisingly
helpful.
