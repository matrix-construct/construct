FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y \
        autoconf \
        autoconf-archive \
        autoconf2.13 \
        automake \
        autotools-dev \
        boost1.74 \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        liblz4-dev \
        libnss-db \
        libmagic-dev \
        libsodium-dev \
        libssl-dev \
        libtool \
        libzstd-dev \
        shtool \
        xz-utils \
	&& update-ca-certificates \
    && apt-get clean \
    && apt-get autoremove --purge -y \
    && rm -rf /var/lib/apt/lists/* \
    && true

ENV ROCKSDB_VERSION=6.11.4
ARG skiprocks

RUN true \
	&& if test -n "$skiprocks"; then \
		exit 0; \
	fi \
    && cd /usr/src \
    && curl -sL https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${ROCKSDB_VERSION} -o rocksdb-${ROCKSDB_VERSION}.tar.gz \
    && tar xfvz rocksdb-${ROCKSDB_VERSION}.tar.gz \
    && rm rocksdb-${ROCKSDB_VERSION}.tar.gz \
    && ln -s /usr/src/rocksdb-${ROCKSDB_VERSION} /usr/src/rocksdb \
    && cd /usr/src/rocksdb-${ROCKSDB_VERSION} \
    && \
    CFLAGS="-g0" \
    LDFLAGS="-Wl,--strip-all" \
    cmake -H. -Bbuild \
		-DCMAKE_RULE_MESSAGES:BOOL=OFF \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
        -DWITH_JNI=0 \
        -DWITH_TESTS=0 \
        -DWITH_BENCHMARK_TOOLS=0 \
        -DWITH_CORE_TOOLS=0 \
        -DFAIL_ON_WARNINGS=0 \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_GFLAGS=0 \
        -DWITH_LIBURING=0 \
        -DWITH_LZ4=1 \
        -DWITH_ZSTD=1 \
        -DUSE_RTTI=1 \
        -DBUILD_SHARED_LIBS=1 \
    && cmake --build build --target install \
    && rm -rf build \
	&& true

RUN mkdir /build
WORKDIR /build
