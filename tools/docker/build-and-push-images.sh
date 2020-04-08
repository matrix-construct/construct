#!/bin/sh
# Please run from the directory this file in
docker pull ubuntu:18.04
docker build -t jevolk/construct:ubuntu-18.04 ./ubuntu/base
docker build -t jevolk/construct:ubuntu-18.04-clang-9 ./ubuntu/clang-9
docker build -t jevolk/construct:ubuntu-18.04-gcc-8 ./ubuntu/gcc-8

docker push jevolk/construct:ubuntu-18.04-clang-9
docker push jevolk/construct:ubuntu-18.04-gcc-8
