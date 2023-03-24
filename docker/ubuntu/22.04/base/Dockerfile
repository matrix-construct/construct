FROM ubuntu:22.04

ARG skiprocks
ARG rocksdb_version 7.4.5
ARG rocksdb_url

ENV rocksdb_version ${rocksdb_version}
ENV rocksdb_url https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocksdb_version}

ENV packages="\
ca-certificates \
libatomic1 \
libjemalloc2 \
libboost-chrono1.74.0 \
libboost-context1.74.0 \
libboost-coroutine1.74.0 \
libboost-system1.74.0 \
libboost-thread1.74.0 \
libicu70 \
libnss-db \
libsodium23 \
libssl3 \
liblz4-1 \
libmagic1 \
libzstd1 \
"

ENV packages_rocksdb_dev="\
build-essential \
cmake \
curl \
git \
libjemalloc-dev \
liblz4-dev \
libzstd-dev \
xz-utils \
"

ENV packages_dev="\
${packages_rocksdb_dev} \
libboost-chrono1.74-dev \
libboost-context1.74-dev \
libboost-coroutine1.74-dev \
libboost-system1.74-dev \
libboost-thread1.74-dev \
libicu-dev \
libmagic-dev \
libsodium-dev \
libssl-dev \
"

ENV rocksdb_cmake="\
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
-DWITH_JEMALLOC=1 \
-DWITH_LZ4=1 \
-DWITH_ZSTD=1 \
-DUSE_RTTI=1 \
-DBUILD_SHARED_LIBS=1 \
"

ENV do_install true \
&& export DEBIAN_FRONTEND=noninteractive \
&& apt-get update \
&& apt-get install --no-install-recommends -y -f

ENV do_purge true \
&& export DEBIAN_FRONTEND=noninteractive \
&& apt-get purge -y

ENV do_clean true \
&& export DEBIAN_FRONTEND=noninteractive \
&& apt-get clean \
&& apt-get autoremove --purge -y \
&& rm -rf /var/lib/apt/lists/* \
&& true

RUN true \
&& eval ${do_install} ${packages} \
&& update-ca-certificates \
&& eval ${do_clean} \
&& mkdir /build \
&& true

WORKDIR /build
RUN true \
&& if test -n "$skiprocks"; then \
	exit 0; \
fi \
&& eval ${do_install} ${packages_rocksdb_dev} \
&& cd /usr/src \
&& curl -sL ${rocksdb_url} -o rocksdb-${rocksdb_version}.tar.gz \
&& tar xfvz rocksdb-${rocksdb_version}.tar.gz \
&& rm rocksdb-${rocksdb_version}.tar.gz \
&& ln -s /usr/src/rocksdb-${rocksdb_version} /usr/src/rocksdb \
&& cd /usr/src/rocksdb-${rocksdb_version} \
&& \
CFLAGS="-g0 -ftls-model=initial-exec" \
LDFLAGS="-Wl,--strip-all" \
cmake -H. -Bbuild ${rocksdb_cmake} \
&& cmake --build build --target install --parallel `nproc` \
&& rm -rf build \
&& rm -rf /usr/lib/$(uname -m)-linux-gnu/librocksdb.a \
&& eval ${do_purge} ${packages_rocksdb_dev} \
&& eval ${do_clean} \
&& true
