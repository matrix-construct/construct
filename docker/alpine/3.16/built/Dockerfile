ARG acct
ARG repo
ARG feature

FROM ${acct}/${repo}:alpine-3.16-${feature}

ARG cc
ARG cxx
ARG extra_packages_dev
ARG extra_packages_dev1
ARG extra_packages_dev2

ENV rocks_version 7.4.3
ENV rocks_url https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocks_version}
ENV ctor_url https://github.com/matrix-construct/construct

ENV CC ${cc}
ENV CXX ${cxx}
ENV CONFIG_SHELL bash

ENV packages_dev="\
${packages_dev} \
autoconf \
autoconf-archive \
autoconf2.13 \
automake \
bash \
binutils-gold \
build-base \
curl \
git \
libtool \
${extra_packages_dev} \
${extra_packages_dev1} \
${extra_packages_dev2} \
"

WORKDIR /usr/src
RUN true \
&& apk add --no-cache ${packages_dev} \
&& curl -sL ${rocks_url} -o rocksdb-${rocks_version}.tar.gz \
&& tar xfz rocksdb-${rocks_version}.tar.gz \
&& rm -v rocksdb-${rocks_version}.tar.gz \
&& mv -v rocksdb-${rocks_version} rocksdb \
&& git clone ${ctor_url} construct \
&& rmdir -v construct/deps/rocksdb \
&& ln -sv /usr/src/rocksdb construct/deps/rocksdb \
&& cd /usr/src/construct \
&& ./autogen.sh \
&& (./configure --with-profile=no || (cat config.log; exit 1)) \
&& make -j `nproc` EXTRA_LDFLAGS="-Wl,--strip-all" install \
&& rm -rf /usr/src/rocksdb \
&& rm -rf /usr/src/construct \
&& rm -rf /usr/include/ircd \
&& rm -rf /usr/share/construct \
&& apk del --purge ${packages_dev} \
&& true
