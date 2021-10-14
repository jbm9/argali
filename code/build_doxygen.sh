#!/bin/bash
#
# A helper shell script to run doxygen with version numbers.
#
# There is no way to pass in doxygen options on the command line, so
# instead we have to do this "rewrite the config file on the fly"
# thing.
#

full_version=$(./argali_version.sh)

[ $DEBUG ] && echo "Building doxygen for ${full_version}"

(cat Doxyfile; echo "PROJECT_NUMBER=${full_version}") | doxygen -

[ $DEBUG ] && echo ">>>>>>>>>>>> Running PDF assembly for ${full_version}"

pushd doxygen_output/latex/
[ $DEBUG ] && pwd
[ $DEBUG ] && ls -l
make
popd
cp doxygen_output/latex/refman.pdf .
