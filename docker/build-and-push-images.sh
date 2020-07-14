#!/bin/sh
BASEDIR=$(dirname "$0")

docker build -t jevolk/construct:ubuntu-20.04 $BASEDIR/ubuntu/base
docker build -t jevolk/construct:ubuntu-20.04-clang-9 $BASEDIR/ubuntu/clang-9
docker build -t jevolk/construct:ubuntu-20.04-clang-10 $BASEDIR/ubuntu/clang-10
docker build -t jevolk/construct:ubuntu-20.04-gcc-8 $BASEDIR/ubuntu/gcc-8
docker build -t jevolk/construct:ubuntu-20.04-gcc-9 $BASEDIR/ubuntu/gcc-9
docker build -t jevolk/construct:ubuntu-20.04-gcc-10 $BASEDIR/ubuntu/gcc-10

docker push jevolk/construct:ubuntu-20.04-clang-9
docker push jevolk/construct:ubuntu-20.04-clang-10
docker push jevolk/construct:ubuntu-20.04-gcc-8
docker push jevolk/construct:ubuntu-20.04-gcc-9
docker push jevolk/construct:ubuntu-20.04-gcc-10
