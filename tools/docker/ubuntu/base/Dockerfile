FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update && \
    apt-get install --no-install-recommends -y software-properties-common && \
    add-apt-repository -y ppa:mhier/libboost-latest && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install --no-install-recommends -y \
        autoconf \
        autoconf-archive \
        autoconf2.13 \
        automake \
        autotools-dev \
        boost1.68 \
        build-essential \
        cmake \
        curl \
        git \
        libbz2-dev \
        libgflags-dev \
        libgraphicsmagick1-dev \
        libjemalloc-dev \
        liblz4-dev \
        libmagic-dev \
        libnss-db \
        libsodium-dev \
        libssl-dev \
        libtool \
        shtool \
        xz-utils && \
    apt-get purge -y software-properties-common && \
    apt-get clean && \
    apt-get autoremove --purge -y && \
    rm -rf /var/lib/apt/lists/*

ENV ROCKSDB_VERSION=5.16.6

RUN \
    mkdir -p /tmpbuild/rocksdb && \
    cd /tmpbuild/rocksdb && \
    curl -sL https://github.com/facebook/rocksdb/archive/v${ROCKSDB_VERSION}.tar.gz -o rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    tar xfvz rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    cd /tmpbuild/rocksdb/rocksdb-${ROCKSDB_VERSION} && \
    cmake -H. -Bbuild \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_TESTS=0 \
        -DWITH_TOOLS=0 \
        -DUSE_RTTI=1 \
        -DWITH_LZ4=1 \
        -DBUILD_SHARED_LIBS=1 && \
    cmake --build build --target install && \
    rm -Rf /tmpbuild/

RUN mkdir /build

WORKDIR /build

