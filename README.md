# argali

Copyright (c) 2021 Josh Myer <josh@joshisanerd.com>

CC0 v1.0

Argali is an STM32-based product development template.  That is, it's
meant to be a starting point for products, not just projects.  This is
a bit of a new concept in the open source space, so it's worth
expanding on it a bit.

## Projects, Products, and Companies

Someone much more clever than me once outlined how he assesses the
scale of a new invention/idea: "Is this a project, a product, or a
company?"  This is a very insightful question, as it helps define the
scope of what you're up to.

Projects are one-offs, with a resulting item or report.  They habe a
closed-ended time span to them, and don't involve ongoing support or
development longer term.  Projects are mostly R&D, art pieces, or
one-off hacks (ie: art pieces for nerds).

Products start as projects, but they wind up "living on," as it were.
All things that are up for sale in quantity are, obviously enough, in
this category.  Less obviously, so are pieces of infrastructure.  This
is part of what made Amazon's AWS possible: in a large enough
organization, any piece of infrastructure is its own product.

Companies are usually built around a single initial product.  However,
that product usually needs to show potential for growth and expansion.
The initial potential is provided by the fact that nobody has one yet:
this alone can allow a single compelling product to go from zero to a
million users.  But usually you need a product that can be riffed on,
and used to serve multiple markets.

## Argali is for products

For any embedded thing that is "project" scale, it's hard to beat
Arduino.  You can very quickly slap something together and see if it's
possible.  From there, though, it's not easy to get to a sustainable
product.  This often leads to an overgrown prototype, which isn't easy
to get into production as a product.  I speak from some experience
here, as I've been brought in as a consultant on multiple of these.

Argali is a template to make STM32 projects easy to start cleanly,
with a pathway to becoming a sustained product.  It's intended for
greenfield work at the moment, so we can make some strong assumptions:

* C only, no C++ support (yet?)
* A clean separation between logic code and hardware code
* Source code will be in `code/src/foo.c`
* Unit test code will be in `code/src/test_foo
* There's a unified development environment that all developers will
  build in (Docker image with make and gcc), but developers are free
  to actually code however they wan
* Everything runs on a Linux machine (or VM), simplifying scripts

## License

Argali is released under the CC0 Licence v1.0,
https://creativecommons.org/publicdomain/zero/1.0/


## Features

* Builds libopencm3 code out of the box (For the F767ZI Nucleo board)
* Runs everything inside a docker container, so versions can be
  well-controlled
* Can run unit tests in Linux on the docker container (using the Unity
  test framework)


## Docker niceties

The docker container runs as your userid, so you won't wind up with
lots of files owned by `root:root` that you need to constantly
`chmod`.

In `code/`, there is `docker_shell.sh`.  Running this will give you a
bash shell inside the container.  The main purpose of this is
troubleshooting, since it's likely that you'll wind up needing to bolt
on a lot of your own stuff.  The other big use for this is running
your Linux-side unit tests under `gdb`.

The docker shell also sets a custom `${HISTFILE}` variable, which
means that you actually get a bash history inside your docker
containers that can persist across sessions.

## Code of Conduct

argali uses the "Contributor Covenant" code of conduct (currently
version 1.3.0).

Please review [CODE_OF_CONDUCT.md] before submitting (computer) code
to the repository.