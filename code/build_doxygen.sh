#!/bin/bash
#
# A helper shell script to run doxygen with version numbers.
#
# There is no way to pass in doxygen options on the command line, so
# instead we have to do this "rewrite the config file on the fly"
# thing.
#

# Also note that we need to have a janky multi-mount, as /code doesn't
# contain its own .git checkout.  This means that the git metadata
# isn't available to us without multiple `-v` arguments to the docker
# run command.
GIT="git -C /project_base"

version=$(cat VERSION)

gitrev=$(${GIT} rev-parse --short HEAD)

# Check if git is dirty by looking at files known to git.
#
# A quick note on why it's sufficient to only check for changes in
# known files: to compile new files into the final binary, they have
# to be added to the Makefile, or included in the source code, or
# similar.  In general, it's not possible to add a new file to a build
# without also touching existing files.  Thus, it's possible to get a
# clean build if a file isn't checked in, the CI tests should catch
# that, as things should fail to compile in that environment.

${GIT} diff-files --quiet
if [ "x$?" = "x1" ];
then
    gitrev="${gitrev}-dirty"
fi

full_version="${version}-${gitrev}"

[ $DEBUG ] && echo "Building doxygen for ${full_version}"

(cat Doxyfile; echo "PROJECT_NUMBER=${full_version}") | doxygen -
