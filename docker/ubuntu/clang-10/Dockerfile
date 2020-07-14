FROM jevolk/construct:ubuntu-20.04

ENV CC clang-10
ENV CXX clang++-10

RUN apt-get update \
   && apt-get install --no-install-recommends -y clang-10 llvm-10-dev \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*

