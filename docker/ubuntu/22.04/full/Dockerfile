FROM jevolk/construct:ubuntu-22.04-base

ENV packages="\
opencl-headers \
libgraphicsmagick1-dev \
libgraphicsmagick-q16-3 \
libicu-dev \
libjemalloc-dev \
libopencl-clang-dev \
libpng-dev \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean \
&& apt-get autoremove --purge -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
