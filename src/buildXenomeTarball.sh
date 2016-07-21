#!/bin/sh set -x

# Create a tarball for a Xenome distribution.
# This file needs to be edited if more data is to be 
# included in the tarball.
# The type of executable is hard coded: release, no popcount and 
# static linking.

# Side effects:
#   directory xenome-VERSION is created.

# TODO:
# 1. add a test directory and files for testing.
# 2. install pandoc and build manuals automatically.

# Program parameter is the version number of goss.
VERSION=$1


# The NAME will be used for a directory to archive.
if [ "x$2" = "xpopcnt" ]
then
    NAME=xenome-${VERSION}-rp
    SOURCE="release-popcnt"
else
    NAME=xenome-${VERSION}-r
    SOURCE="release"
fi
echo $NAME

test ! -d $NAME && mkdir $NAME
test ! -d $NAME/example && mkdir $NAME/example
#mkdir $NAME/test

# Copy the required files into the staging directory.
cp -f bin/gcc-4.4.3/${SOURCE}/debug-symbols-on/link-static/runtime-link-static/threading-multi/xenome $NAME
strip $NAME/xenome

cp -f ../docs/xenome.pdf   $NAME
cp -f ../docs/xenome.1     $NAME
cp -f ../docs/license.txt   $NAME
cp -f ../docs/xenomeREADME.txt    $NAME/README.txt

tar cvfz ${NAME}.tar.gz $NAME
