FROM jevolk/construct:ubuntu-18.04

ENV CXX g++-9
ENV CC gcc-9

RUN apt-get update \
   && apt-get install --no-install-recommends -y software-properties-common \
   && add-apt-repository ppa:jonathonf/gcc-9.2 \
   && apt-get update \
   && apt-get install --no-install-recommends -y g++- gcc-9 \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*
