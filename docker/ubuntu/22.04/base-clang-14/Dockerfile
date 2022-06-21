FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-14
ENV CXX clang++-14

RUN apt-get update \
   && apt-get install --no-install-recommends -y clang-14 llvm-14-dev \
   && apt-get clean -y \
   && rm -rf /var/lib/apt/lists/*

