FROM jevolk/construct:ubuntu-22.04-base

ENV DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update && \
    apt-get install --no-install-recommends -y \
        opencl-headers \
        libgraphicsmagick1-dev \
        libicu-dev \
        libjemalloc-dev \
        libopencl-clang-dev \
        libpng-dev && \
    apt-get clean && \
    apt-get autoremove --purge -y && \
    rm -rf /var/lib/apt/lists/*
