ARG dist_name ubuntu
ARG dist_version 22.04

FROM --platform=$TARGETPLATFORM ${dist_name}:${dist_version}

ARG skiprocks
ARG rocksdb_version 7.4.5
ARG rocksdb_url
ARG rocksdb_avx 0
ARG rocksdb_avx2 0
ARG boost_version 1.74
ARG icu_version 70
ARG nprocs 1

ENV rocksdb_version ${rocksdb_version}
ENV rocksdb_url https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocksdb_version}
ENV rocksdb_avx ${rocksdb_avx}
ENV rocksdb_avx2 ${rocksdb_avx2}
ENV boost_version ${boost_version}
ENV icu_version ${icu_version}
ENV nprocs ${nprocs}

ENV packages="\
ca-certificates \
libatomic1 \
libjemalloc2 \
libboost-chrono${boost_version}.0 \
libboost-context${boost_version}.0 \
libboost-coroutine${boost_version}.0 \
libboost-system${boost_version}.0 \
libboost-thread${boost_version}.0 \
libicu${icu_version} \
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
libjemalloc-dev \
liblz4-dev \
libzstd-dev \
xz-utils \
"

ENV packages_dev="\
${packages_rocksdb_dev} \
libboost-chrono${boost_version}-dev \
libboost-context${boost_version}-dev \
libboost-coroutine${boost_version}-dev \
libboost-system${boost_version}-dev \
libboost-thread${boost_version}-dev \
libicu-dev \
libmagic-dev \
libsodium-dev \
libssl-dev \
"

ENV rocksdb_cmake="\
-DCMAKE_RULE_MESSAGES:BOOL=OFF \
-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
-DCMAKE_BUILD_TYPE=Release \
-DBUILD_SHARED_LIBS=1 \
-DFAIL_ON_WARNINGS=0 \
-DUSE_RTTI=1 \
-DPORTABLE=1 \
-DFORCE_AVX=${rocksdb_avx} \
-DFORCE_AVX2=${rocksdb_avx2} \
-DWITH_JNI=0 \
-DWITH_TESTS=0 \
-DWITH_BENCHMARK_TOOLS=0 \
-DWITH_CORE_TOOLS=0 \
-DWITH_GFLAGS=0 \
-DWITH_LIBURING=0 \
-DWITH_JEMALLOC=1 \
-DWITH_LZ4=1 \
-DWITH_ZSTD=1 \
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

ENV do_fetch_rocksdb true \
&& eval ${do_install} curl \
&& cd /usr/src \
&& curl -sL ${rocksdb_url} -o rocksdb-${rocksdb_version}.tar.gz \
&& tar xfz rocksdb-${rocksdb_version}.tar.gz \
&& rm rocksdb-${rocksdb_version}.tar.gz \
&& mv /usr/src/rocksdb-${rocksdb_version} /usr/src/rocksdb \
&& cd - \
&& eval ${do_purge} curl \
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
&& eval ${do_fetch_rocksdb} \
&& cd /usr/src/rocksdb \
&& \
CFLAGS="-g0 -ftls-model=initial-exec" \
LDFLAGS="-Wl,--strip-all" \
cmake -H. -Bbuild ${rocksdb_cmake} \
&& cmake --build build --target install --parallel ${nprocs} \
&& rm -rf /usr/lib/$(uname -m)-linux-gnu/librocksdb.a \
&& rm -rf /usr/src/rocksdb \
&& eval ${do_purge} ${packages_rocksdb_dev} \
&& eval ${do_clean} \
&& true
