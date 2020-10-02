FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update && \
    apt-get install --no-install-recommends -y software-properties-common && \
    apt-get update && \
    apt-get install --no-install-recommends -y \
        autoconf \
        autoconf-archive \
        autoconf2.13 \
        automake \
        autotools-dev \
        boost1.71 \
        build-essential \
        cmake \
        curl \
        git \
        libbz2-dev \
        libgraphicsmagick1-dev \
        libicu-dev \
        libjemalloc-dev \
        libmagic-dev \
        libnss-db \
        libsodium-dev \
        libssl-dev \
        libtool \
        libzstd-dev \
        shtool \
        xz-utils && \
    apt-get purge -y software-properties-common && \
    apt-get clean && \
    apt-get autoremove --purge -y && \
    rm -rf /var/lib/apt/lists/*

ENV ROCKSDB_VERSION=6.6.4

RUN \
    cd /usr/src && \
    curl -sL https://github.com/facebook/rocksdb/archive/v${ROCKSDB_VERSION}.tar.gz -o rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    tar xfvz rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    rm rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    ln -s /usr/src/rocksdb-${ROCKSDB_VERSION} /usr/src/rocksdb && \
    cd /usr/src/rocksdb-${ROCKSDB_VERSION} && \
    CFLAGS="-g0" \
    cmake -H. -Bbuild \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_TESTS=0 \
        -DWITH_TOOLS=0 \
        -DUSE_RTTI=1 \
        -DWITH_LZ4=1 \
        -DWITH_GFLAGS=0 \
        -DBUILD_SHARED_LIBS=1 && \
    cmake --build build --target install && \
    rm -rf build

RUN mkdir /build
WORKDIR /build
