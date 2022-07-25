#!/bin/sh
BASEDIR=$(dirname "$0")
ACCT=jevolk
REPO=construct
#ARGS="--build-arg skiprocks=1"

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

docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base $BASEDIR/ubuntu/22.04/base
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full $BASEDIR/ubuntu/22.04/full
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-8 $BASEDIR/ubuntu/22.04/base-gcc-8
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-9 $BASEDIR/ubuntu/22.04/base-gcc-9
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-10 $BASEDIR/ubuntu/22.04/base-gcc-10
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-11 $BASEDIR/ubuntu/22.04/base-gcc-11
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-gcc-12 $BASEDIR/ubuntu/22.04/base-gcc-12
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-10 $BASEDIR/ubuntu/22.04/base-clang-10
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-11 $BASEDIR/ubuntu/22.04/base-clang-11
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-12 $BASEDIR/ubuntu/22.04/base-clang-12
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-13 $BASEDIR/ubuntu/22.04/base-clang-13
#docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-base-clang-14 $BASEDIR/ubuntu/22.04/base-clang-14
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-full-clang-14 $BASEDIR/ubuntu/22.04/full-clang-14
docker build $ARGS -t $ACCT/$REPO:ubuntu-22.04-built $BASEDIR/ubuntu/22.04/built

docker push $ACCT/$REPO:ubuntu-22.04-base
docker push $ACCT/$REPO:ubuntu-22.04-full
#docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-8
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-9
#docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-10
#docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-11
docker push $ACCT/$REPO:ubuntu-22.04-base-gcc-12
#docker push $ACCT/$REPO:ubuntu-22.04-base-clang-10
#docker push $ACCT/$REPO:ubuntu-22.04-base-clang-11
#docker push $ACCT/$REPO:ubuntu-22.04-base-clang-12
#docker push $ACCT/$REPO:ubuntu-22.04-base-clang-13
#docker push $ACCT/$REPO:ubuntu-22.04-base-clang-14
docker push $ACCT/$REPO:ubuntu-22.04-full-clang-14
docker push $ACCT/$REPO:ubuntu-22.04-built

docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base $BASEDIR/alpine/3.16/base
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full $BASEDIR/alpine/3.16/full
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-base-built $BASEDIR/alpine/3.16/base-built
docker build $ARGS -t $ACCT/$REPO:alpine-3.16-full-built $BASEDIR/alpine/3.16/full-built

docker push $ACCT/$REPO:alpine-3.16-base
docker push $ACCT/$REPO:alpine-3.16-full
docker push $ACCT/$REPO:alpine-3.16-base-built
docker push $ACCT/$REPO:alpine-3.16-full-built
