#!/bin/bash

# do the real building work
# this script is executed on build VM

set -x

cd ./MaxScale

git submodule update --init

NCPU=$(grep -c processor /proc/cpuinfo)

if [ "$PARALLEL_BUILD" == "no" ]
then
    NCPU=1
fi

mkdir _build
cd _build
cmake ..  $cmake_flags
make -j${NCPU} || exit 1

if [[ "$cmake_flags" =~ "BUILD_TESTS=Y" ]]
then
    # We don't care about memory leaks in the tests (e.g. servers are never freed)
    export ASAN_OPTIONS=detect_leaks=0
    # All tests must pass otherwise the build is considered a failure
    ctest --timeout 120 --output-on-failure -j${NCPU} || exit 1

    # See if docker is installed and run REST API and MaxCtrl tests if it is
    command -v docker
    if [ $? -eq 0 ]
    then
        export SKIP_SHUTDOWN=Y
        make -j${NCPU} test_rest_api && make -j${NCPU} test_maxctrl
        rc=$?
        #docker ps -aq|xargs docker rm -vf

        if [ $rc -ne 0 ]
        then
            cat maxscale_test/*.output maxscale_test/log/maxscale/*.log
            exit 1
        fi
    fi
fi

# Never strip binaries
sudo rm -rf /usr/bin/strip
sudo touch /usr/bin/strip
sudo chmod a+x /usr/bin/strip

sudo make -j${NCPU} package
res=$?
if [ $res != 0 ] ; then
	echo "Make package failed"
	exit $res
fi

sudo rm ../CMakeCache.txt
sudo rm CMakeCache.txt

echo "Building tarball..."
cmake .. $cmake_flags -DTARBALL=Y
sudo make -j${NCPU} package

cd ..
cp _build/*.rpm .
cp _build/*.gz .

sudo rpm -i maxscale*.rpm
