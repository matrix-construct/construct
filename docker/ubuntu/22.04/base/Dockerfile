FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ENV packages="\
autoconf \
autoconf-archive \
autoconf2.13 \
automake \
autotools-dev \
build-essential \
ca-certificates \
cmake \
curl \
git \
libboost-chrono1.74-dev \
libboost-context1.74-dev \
libboost-coroutine1.74-dev \
libboost-system1.74-dev \
libboost-thread1.74-dev \
liblz4-dev \
libnss-db \
libmagic-dev \
libsodium-dev \
libssl-dev \
libtool \
libzstd-dev \
shtool \
xz-utils \
"

ENV purges="\
cmake \
curl \
"

ENV rocks_version=6.11.4
ENV rocks_cmake="\
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
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages}\
&& update-ca-certificates \
&& apt-get clean \
&& apt-get autoremove --purge -y \
&& rm -rf /var/lib/apt/lists/* \
&& mkdir /build \
&& true

ARG skiprocks
RUN true \
&& if test -n "$skiprocks"; then \
	exit 0; \
fi \
&& cd /usr/src \
&& curl -sL https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocks_version} -o rocksdb-${rocks_version}.tar.gz \
&& tar xfvz rocksdb-${rocks_version}.tar.gz \
&& rm rocksdb-${rocks_version}.tar.gz \
&& ln -s /usr/src/rocksdb-${rocks_version} /usr/src/rocksdb \
&& cd /usr/src/rocksdb-${rocks_version} \
&& \
CFLAGS="-g0" \
LDFLAGS="-Wl,--strip-all" \
cmake -H. -Bbuild ${rocks_cmake} \
&& cmake --build build --target install --parallel `nproc` \
&& rm -rf build \
&& apt-get purge -y ${purges} \
&& apt-get clean \
&& apt-get autoremove --purge -y \
&& rm -rf /var/lib/apt/lists/* \
&& true

WORKDIR /build
