FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-12
ENV CXX clang++-12

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y clang-12 llvm-12-dev \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true

