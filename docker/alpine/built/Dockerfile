ARG acct
ARG repo
ARG dist_name
ARG dist_version
ARG feature
ARG machine

FROM ${acct}/${repo}:${dist_name}-${dist_version}-${feature}-${machine}

ARG cc
ARG cxx
ARG extra_packages_dev
ARG rocksdb_version 7.4.3
ARG rocksdb_url
ARG ctor_url https://github.com/matrix-construct/construct
ARG machine_spec
ARG nprocs

ENV CC ${cc}
ENV CXX ${cxx}
ENV CONFIG_SHELL bash

ENV rocksdb_version ${rocksdb_version}
ENV rocksdb_url https://codeload.github.com/facebook/rocksdb/tar.gz/refs/tags/v${rocksdb_version}
ENV ctor_url ${ctor_url}
ENV machine_spec ${machine_spec}
ENV nprocs ${nprocs}

ENV packages_dev="\
${packages_dev} \
autoconf \
autoconf-archive \
automake \
bash \
binutils-gold \
build-base \
curl \
git \
libtool \
${extra_packages_dev} \
"

WORKDIR /usr/src
RUN true \
&& apk add --no-cache ${packages_dev} \
&& curl -sL ${rocksdb_url} -o rocksdb-${rocksdb_version}.tar.gz \
&& tar xfz rocksdb-${rocksdb_version}.tar.gz \
&& rm -v rocksdb-${rocksdb_version}.tar.gz \
&& mv -v rocksdb-${rocksdb_version} rocksdb \
&& git clone ${ctor_url} construct \
&& rmdir -v construct/deps/rocksdb \
&& ln -sv /usr/src/rocksdb construct/deps/rocksdb \
&& cd /usr/src/construct \
&& ./autogen.sh \
&& (./configure --disable-iou --enable-generic --with-machine="${machine_spec}" || (cat config.log; exit 1)) \
&& make -j ${nprocs} EXTRA_LDFLAGS="-Wl,--strip-all" install \
&& rm -rf /usr/src/rocksdb \
&& rm -rf /usr/src/construct \
&& rm -rf /usr/include/ircd \
&& rm -rf /usr/share/construct \
&& apk del --purge ${packages_dev} \
&& true
