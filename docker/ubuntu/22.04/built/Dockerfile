FROM jevolk/construct:ubuntu-22.04-full-clang-14

ENV purges="\
autoconf \
autoconf-archive \
autoconf2.13 \
automake \
autotools-dev \
build-essential \
clang-14 \
git \
libtool \
llvm-14-dev \
shtool \
"

RUN true \
&& git clone https://github.com/jevolk/charybdis.git \
&& cd charybdis \
&& rmdir -v deps/rocksdb \
&& ln -sv /usr/src/rocksdb deps \
&& ./autogen.sh \
&& ./configure \
&& make -j `nproc` \
&& make install \
&& cd .. \
&& rm -rf charybdis \
&& apt-get purge -y ${purges} \
&& apt-get autoremove --purge -y \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
