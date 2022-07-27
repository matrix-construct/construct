#!/bin/sh

BASEDIR=$(dirname "$0")
ACCT=jevolk
REPO=construct

ARGS_=""
ARGS_="$ARGS_ --compress=true"
ARGS_="$ARGS_ --build-arg acct=$ACCT"
ARGS_="$ARGS_ --build-arg repo=$REPO"

###############################################################################
#
# Alpine 3.16
#

#
# L0 - Base featured image
#

ARGS="$ARGS_"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base $BASEDIR/alpine/3.16/base

#
# L1 - Fully featured image
#

ARGS="$ARGS_"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full $BASEDIR/alpine/3.16/full

#
# L3 - Built images
#

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-gcc $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
ARGS="$ARGS -t $ACCT/$REPO:alpine-3.16-base-built"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built-clang $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=gcc"
ARGS="$ARGS --build-arg extra_packages_dev1=g++"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-gcc $BASEDIR/alpine/3.16/built

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg extra_packages_dev=clang"
ARGS="$ARGS --build-arg extra_packages_dev1=llvm"
ARGS="$ARGS --build-arg extra_packages_dev2=llvm-dev"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
ARGS="$ARGS -t $ACCT/$REPO:alpine-3.16-full-built"
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built-clang $BASEDIR/alpine/3.16/built

#
# L4 - Test images
#

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
#docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-gcc $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=base"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
#docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-test-clang $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg cc=gcc --build-arg cxx=g++"
ARGS="$ARGS -t $ACCT/$REPO:alpine-3.16-full-test"
#docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-test-gcc $BASEDIR/alpine/3.16/test

ARGS="$ARGS_"
ARGS="$ARGS --build-arg feature=full"
ARGS="$ARGS --build-arg cc=clang --build-arg cxx=clang++"
#docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-test-clang $BASEDIR/alpine/3.16/test

#
# Pushed images
#

#docker push $ACCT/$REPO:alpine-3.16-base
#docker push $ACCT/$REPO:alpine-3.16-base-dev
docker push $ACCT/$REPO:alpine-3.16-base-built-gcc
docker push $ACCT/$REPO:alpine-3.16-base-built-clang
docker push $ACCT/$REPO:alpine-3.16-base-built

#docker push $ACCT/$REPO:alpine-3.16-full
#docker push $ACCT/$REPO:alpine-3.16-full-dev
docker push $ACCT/$REPO:alpine-3.16-full-built-gcc
docker push $ACCT/$REPO:alpine-3.16-full-built-clang
docker push $ACCT/$REPO:alpine-3.16-full-built

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
