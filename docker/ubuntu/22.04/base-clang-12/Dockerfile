FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-12
ENV CXX clang++-12

ENV packages="\
clang-12 \
llvm-12-dev \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
