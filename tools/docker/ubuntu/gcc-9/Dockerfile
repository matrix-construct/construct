FROM jevolk/construct:ubuntu-20.04

ENV CXX g++-9
ENV CC gcc-9

RUN apt-get update \
   && apt-get install --no-install-recommends -y software-properties-common \
   && apt-get update \
   && apt-get install --no-install-recommends -y g++-9 gcc-9 \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*
