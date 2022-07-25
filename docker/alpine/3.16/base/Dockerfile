FROM alpine:3.16.1

ENV packages="\
boost-chrono \
boost-context \
boost-coroutine \
boost-system \
boost-thread \
ca-certificates \
icu \
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
&& mkdir /build \
&& true

WORKDIR /build
