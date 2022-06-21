FROM jevolk/construct:ubuntu-20.04

ENV CXX g++-10
ENV CC gcc-10

RUN apt-get update \
   && apt-get install --no-install-recommends -y software-properties-common \
   && apt-get update \
   && apt-get install --no-install-recommends -y g++-10 gcc-10 \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*
