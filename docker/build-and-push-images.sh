#!/bin/sh

BASEDIR=$(dirname "$0")
ACCT=jevolk
REPO=construct
ctor_url="https://github.com/matrix-construct/construct"
rocksdb_version=7.4.3

ARGS_=""
ARGS_="$ARGS_ --compress=true"
ARGS_="$ARGS_ --build-arg acct=$ACCT"
ARGS_="$ARGS_ --build-arg repo=$REPO"
ARGS_="$ARGS_ --build-arg ctor_url=$ctor_url"
ARGS_="$ARGS_ --build-arg rocksdb_version=$rocksdb_version"

export DOCKER_BUILDKIT=1
#export BUILDKIT_PROGRESS=plain

###############################################################################
#
# Alpine 3.16
#

#
# L0 - Base featured image
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-amd64 $BASEDIR/alpine/3.16/base

#
# L1 - Fully featured image
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-amd64 $BASEDIR/alpine/3.16/full

#
# L2/L3 - Built/Test images
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-gcc-amd64 $BASEDIR/alpine/3.16/built
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-gcc-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-clang-amd64 $BASEDIR/alpine/3.16/built
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-clang-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-gcc-amd64 $BASEDIR/alpine/3.16/built
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-test-gcc-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-clang-amd64 $BASEDIR/alpine/3.16/built
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-test-clang-amd64 $BASEDIR/alpine/3.16/test

#
# Pushed images
#

docker push $ACCT/$REPO:alpine-3.16-base-built-gcc-amd64
docker push $ACCT/$REPO:alpine-3.16-base-built-clang-amd64

docker push $ACCT/$REPO:alpine-3.16-full-built-gcc-amd64
docker push $ACCT/$REPO:alpine-3.16-full-built-clang-amd64

###############################################################################
#
# Ubuntu 22.04
#

#
# L0 - Base featured image
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-amd64 $BASEDIR/ubuntu/22.04/base

#
# L1 - Fully featured image
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-amd64 $BASEDIR/ubuntu/22.04/full

#
# L2/L3 - Build/Built/Test
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=gcc-9"
ARGS="$ARGS --build-arg extra_packages_dev1=g++-9"
ARGS="$ARGS --build-arg cc=gcc-9 --build-arg cxx=g++-9"
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-build-gcc-9-amd64 $BASEDIR/ubuntu/22.04/build
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-built-gcc-9-amd64 $BASEDIR/ubuntu/22.04/built
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-test-gcc-9-amd64 $BASEDIR/ubuntu/22.04/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=gcc-12"
ARGS="$ARGS --build-arg extra_packages_dev1=g++-12"
ARGS="$ARGS --build-arg cc=gcc-12 --build-arg cxx=g++-12"
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-build-gcc-12-amd64 $BASEDIR/ubuntu/22.04/build
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-built-gcc-12-amd64 $BASEDIR/ubuntu/22.04/built
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-test-gcc-12-amd64 $BASEDIR/ubuntu/22.04/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=clang-14"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm-14-dev"
ARGS="$ARGS --build-arg cc=clang-14 --build-arg cxx=clang++-14"
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-build-clang-14-amd64 $BASEDIR/ubuntu/22.04/build
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-built-clang-14-amd64 $BASEDIR/ubuntu/22.04/built
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-test-clang-14-amd64 $BASEDIR/ubuntu/22.04/test

#
# Pushed images
#

docker push $ACCT/$REPO:ubuntu-22.04-base-build-gcc-9-amd64
docker push $ACCT/$REPO:ubuntu-22.04-base-built-gcc-9-amd64

docker push $ACCT/$REPO:ubuntu-22.04-base-build-gcc-12-amd64
docker push $ACCT/$REPO:ubuntu-22.04-base-built-gcc-12-amd64

docker push $ACCT/$REPO:ubuntu-22.04-full-build-clang-14-amd64
docker push $ACCT/$REPO:ubuntu-22.04-full-built-clang-14-amd64

###############################################################################
#
# Ubuntu 20.04
#

#docker build -t $ACCT/$REPO:ubuntu-20.04 $BASEDIR/ubuntu/20.04/base
#docker build -t $ACCT/$REPO:ubuntu-20.04-gcc-8 $BASEDIR/ubuntu/20.04/gcc-8
#docker build -t $ACCT/$REPO:ubuntu-20.04-gcc-9 $BASEDIR/ubuntu/20.04/gcc-9
#docker build -t $ACCT/$REPO:ubuntu-20.04-gcc-10 $BASEDIR/ubuntu/20.04/gcc-10
#docker build -t $ACCT/$REPO:ubuntu-20.04-clang-9 $BASEDIR/ubuntu/20.04/clang-9
#docker build -t $ACCT/$REPO:ubuntu-20.04-clang-10 $BASEDIR/ubuntu/20.04/clang-10
#docker build -t $ACCT/$REPO:ubuntu-20.04-clang-11 $BASEDIR/ubuntu/20.04/clang-11
#docker build -t $ACCT/$REPO:ubuntu-20.04-clang-12 $BASEDIR/ubuntu/20.04/clang-12

#docker push $ACCT/$REPO:ubuntu-20.04-gcc-8
#docker push $ACCT/$REPO:ubuntu-20.04-gcc-9
#docker push $ACCT/$REPO:ubuntu-20.04-gcc-10
#docker push $ACCT/$REPO:ubuntu-20.04-clang-9
#docker push $ACCT/$REPO:ubuntu-20.04-clang-10
#docker push $ACCT/$REPO:ubuntu-20.04-clang-11
#docker push $ACCT/$REPO:ubuntu-20.04-clang-12
