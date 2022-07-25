FROM jevolk/construct:alpine-3.16-base

ENV packages="\
${packages_dev} \
autoconf \
autoconf-archive \
autoconf2.13 \
automake \
bash \
binutils-gold \
curl \
gcc \
g++ \
git \
libtool \
make \
"

ENV CC gcc
ENV CXX g++
ENV CONFIG_SHELL /bin/bash
ENV rocks_version 7.4.3

RUN true \
&& apk add --no-cache ${packages} \
&& mkdir -p /usr/src \
&& cd /usr/src \
&& curl -sL https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocks_version} -o rocksdb-${rocks_version}.tar.gz \
&& tar xfz rocksdb-${rocks_version}.tar.gz \
&& git clone https://github.com/matrix-construct/construct construct \
&& cd construct \
&& rmdir -v deps/rocksdb \
&& ln -sv /usr/src/rocksdb-${rocks_version} deps/rocksdb \
&& ./autogen.sh \
&& ./configure \
&& make -j `nproc` EXTRA_LDFLAGS="-Wl,--strip-all -Wl,--gc-sections" \
&& make install \
&& rm /usr/src/rocksdb-${rocks_version}.tar.gz \
&& rm -rf /usr/src/rocksdb-${rocks_version} \
&& rm -rf /usr/src/construct \
&& apk del --purge ${packages} \
&& true

RUN true \
&& construct -smoketest -debug -nomatrix \
&& true
