#!/bin/sh set -x

# Create a tarball for a goss distribution.
# This file needs to be edited if more data is to be 
# included in the tarball.
# The type of executable is hard coded: release, no popcount and 
# static linking.

# Side effects:
#   directory goss-VERSION is created.

# TODO:
# 1. add a test directory and files for testing.
# 2. install pandoc and build manuals automatically.

# Program parameter is the version number of goss.
VERSION=$1


# The NAME will be used for a directory to archive.
if [ "x$2" = "xpopcnt" ]
then
    NAME=goss-${VERSION}-rp
    SOURCE="release-popcnt"
else
    NAME=goss-${VERSION}-r
    SOURCE="release"
fi
echo $NAME

test ! -d $NAME && mkdir $NAME
test ! -d $NAME/example && mkdir $NAME/example
#mkdir $NAME/test

# Copy the required files into the staging directory.
cp -f bin/gcc-4.4.3/${SOURCE}/debug-symbols-on/link-static/runtime-link-static/threading-multi/goss $NAME
strip $NAME/goss

cp -f ../utils/gossple.sh   $NAME
cp -f ../docs/goss.pdf   $NAME
cp -f ../docs/goss.1     $NAME
cp -f ../docs/gossple.pdf   $NAME
cp -f ../docs/gossple.1     $NAME
cp -f ../docs/license.txt   $NAME
cp -f ../docs/README.txt    $NAME
cp -f ../docs/example/reads1.fq.gz $NAME/example
cp -f ../docs/example/reads2.fq.gz $NAME/example
cp -f ../docs/example/reference.fa $NAME/example
cp -f ../docs/example/build.sh $NAME/example


tar cvfz ${NAME}.tar.gz $NAME

