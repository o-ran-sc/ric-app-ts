#!/usr/bin/env ksh

#	Mnemonic:	rmr_build.sh
#	Abstract:	This will pull RMR from the repo, build and install. This
#				is en lieu of using wget to fetch the RMR package from some
#				repo and installing it.  The package method is preferred
#				but if that breaks this can be used in place of it.

rmr_ver=${1:-3.6.2}

# assume that we're in the proper directory
set -e
git clone "https://gerrit.o-ran-sc.org/r/ric-plt/lib/rmr"

cd rmr
git checkout $ver
mkdir .build
cd .build
cmake .. -DDEV_PKG=1 -DPACK_EXTERNALS=1
make install
cmake .. -DDEV_PKG=0
make install

