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
ARG ctor_url https://github.com/matrix-construct/construct
ARG machine_spec
ARG nprocs

ENV CC ${cc}
ENV CXX ${cxx}
ENV ctor_url ${ctor_url}
ENV machine_spec ${machine_spec}
ENV nprocs ${nprocs}

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
"

RUN true \
&& eval ${do_install} ${packages_dev} \
&& eval ${do_fetch_rocksdb} \
&& git clone ${ctor_url} construct \
&& cd construct \
&& rmdir -v deps/rocksdb \
&& ln -sv /usr/src/rocksdb deps \
&& ./autogen.sh \
&& (./configure --enable-generic --with-machine="${machine_spec}" || (cat config.log; exit 1)) \
&& make -j ${nprocs} \
&& make install \
&& cd .. \
&& rm -rf construct \
&& rm -rf /usr/src/rocksdb \
&& eval ${do_purge} ${packages_dev} \
&& eval ${do_clean} \
&& true
