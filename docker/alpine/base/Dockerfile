ARG dist_name alpine
ARG dist_version 3.16

FROM --platform=$TARGETPLATFORM ${dist_name}:${dist_version}

ENV packages="\
boost-chrono \
boost-context \
boost-coroutine \
boost-system \
boost-thread \
ca-certificates \
icu \
libatomic \
libmagic \
libsodium \
libstdc++ \
lz4 \
openssl \
rocksdb \
zstd \
"

ENV packages_dev="\
boost-dev \
file-dev \
icu-dev \
libsodium-dev \
lz4-dev \
openssl-dev \
rocksdb-dev \
zstd-dev \
"

RUN true \
&& apk add --no-cache ${packages} \
&& true
