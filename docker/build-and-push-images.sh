#!/bin/sh

BASEDIR=$(dirname "$0")
ACCT=jevolk
REPO=construct

ARGS_=""
ARGS_="$ARGS_ --compress=true"
ARGS_="$ARGS_ --build-arg acct=$ACCT"
ARGS_="$ARGS_ --build-arg repo=$REPO"

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
# L2 - Built images
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-gcc-amd64 $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-clang-amd64 $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-gcc-amd64 $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-clang-amd64 $BASEDIR/alpine/3.16/built

#
# L3 - Test images
#

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-gcc-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-clang-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-test-gcc-amd64 $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --platform linux/amd64"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
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

docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base $BASEDIR/ubuntu/22.04/base
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full $BASEDIR/ubuntu/22.04/full
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-9 $BASEDIR/ubuntu/22.04/base-gcc-9
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-10 $BASEDIR/ubuntu/22.04/base-gcc-10
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-11 $BASEDIR/ubuntu/22.04/base-gcc-11
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-12 $BASEDIR/ubuntu/22.04/base-gcc-12
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-10 $BASEDIR/ubuntu/22.04/base-clang-10
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-11 $BASEDIR/ubuntu/22.04/base-clang-11
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-12 $BASEDIR/ubuntu/22.04/base-clang-12
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-13 $BASEDIR/ubuntu/22.04/base-clang-13
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-14 $BASEDIR/ubuntu/22.04/base-clang-14
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-clang-14 -t $ACCT/$REPO:ubuntu-22.04-built $BASEDIR/ubuntu/22.04/full-clang-14

docker push $ACCT/$REPO:ubuntu-22.04-base
docker push $ACCT/$REPO:ubuntu-22.04-full
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-9
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-10
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-11
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-12
docker push $ACCT/$REPO:ubuntu-22.04-base-clang-10
docker push $ACCT/$REPO:ubuntu-22.04-base-clang-11
docker push $ACCT/$REPO:ubuntu-22.04-base-clang-12
docker push $ACCT/$REPO:ubuntu-22.04-base-clang-13
docker push $ACCT/$REPO:ubuntu-22.04-base-clang-14
docker push $ACCT/$REPO:ubuntu-22.04-full-clang-14
docker push $ACCT/$REPO:ubuntu-22.04-built

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
