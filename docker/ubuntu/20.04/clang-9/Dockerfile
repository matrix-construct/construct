FROM jevolk/construct:ubuntu-20.04

ENV CC clang-9
ENV CXX clang++-9

RUN apt-get update \
   && apt-get install --no-install-recommends -y clang-9 llvm-9-dev \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*

