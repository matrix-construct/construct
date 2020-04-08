FROM jevolk/construct:ubuntu-18.04

ENV CC clang-9
ENV CXX clang++-9

RUN apt-get update \
   && apt-get install --no-install-recommends -y clang-9 \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*

