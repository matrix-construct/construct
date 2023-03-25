ARG acct
ARG repo
ARG feature

FROM ${acct}/${repo}:ubuntu-22.04-${feature}-${TARGETARCH}

ARG cc
ARG cxx
ARG extra_packages_dev
ARG extra_packages_dev1
ARG extra_packages_dev2
ARG ctor_url https://github.com/matrix-construct/construct

ENV CC ${cc}
ENV CXX ${cxx}
ENV ctor_url ${ctor_url}

ENV packages_dev="\
${packages_dev} \
autoconf \
autoconf-archive \
autoconf2.13 \
automake \
autotools-dev \
git \
libtool \
shtool \
${extra_packages_dev} \
${extra_packages_dev1} \
${extra_packages_dev2} \
"

RUN true \
&& eval ${do_install} ${packages_dev} \
&& eval ${do_fetch_rocksdb} \
&& git clone ${ctor_url} construct \
&& cd construct \
&& rmdir -v deps/rocksdb \
&& ln -sv /usr/src/rocksdb deps \
&& ./autogen.sh \
&& ./configure \
&& make -j `nproc` \
&& make install \
&& cd .. \
&& rm -rf construct \
&& rm -rf /usr/src/rocksdb \
&& eval ${do_purge} ${packages_dev} \
&& eval ${do_clean} \
&& true
