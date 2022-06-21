FROM jevolk/construct:ubuntu-20.04

ENV CC clang-11
ENV CXX clang++-11

RUN apt-get update \
   && apt-get install --no-install-recommends -y clang-11 llvm-11-dev \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*

