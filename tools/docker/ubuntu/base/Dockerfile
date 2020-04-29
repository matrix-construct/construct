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
        libgflags-dev \
        libgraphicsmagick1-dev \
        libjemalloc-dev \
        liblz4-dev \
        libmagic-dev \
        libnss-db \
        librocksdb-dev \
        libsodium-dev \
        libssl-dev \
        libtool \
        shtool \
        xz-utils && \
    apt-get purge -y software-properties-common && \
    apt-get clean && \
    apt-get autoremove --purge -y && \
    rm -rf /var/lib/apt/lists/*

ENV ROCKSDB_VERSION=5.17.2

RUN \
    cd /usr/src && \
    curl -sL https://github.com/facebook/rocksdb/archive/v${ROCKSDB_VERSION}.tar.gz -o rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    tar xfvz rocksdb-${ROCKSDB_VERSION}.tar.gz && \
    ln -s rocksdb-${ROCKSDB_VERSION} rocksdb && \
    rm rocksdb-${ROCKSDB_VERSION}.tar.gz

RUN mkdir /build
WORKDIR /build

