FROM ubuntu:18.04

RUN \
    apt-get update && \
    apt-get install -y software-properties-common && \
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
        g++-8 \
        gcc-8 \
        libbz2-dev \
        libgflags-dev \
        libmagic-dev \
        libsnappy-dev \
        libsodium-dev \
        libssl1.0-dev \
        libtool \
        libzstd-dev \
        shtool \
        xz-utils \
        zlib1g-dev && \
    apt-get clean && \
    apt-get autoremove --purge -y

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
        -DWITH_ZLIB=1 \
        -DWITH_SNAPPY=1 \
        -DBUILD_SHARED_LIBS=1 && \
    cmake --build build --target install && \
    rm -Rf /tmpbuild/

RUN mkdir /build

ENV CXX g++-8
ENV CC gcc-8

WORKDIR /build

CMD [ "/bin/bash" ]
