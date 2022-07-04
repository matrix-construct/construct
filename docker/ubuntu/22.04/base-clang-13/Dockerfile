FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-13
ENV CXX clang++-13

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y clang-13 llvm-13-dev \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true

