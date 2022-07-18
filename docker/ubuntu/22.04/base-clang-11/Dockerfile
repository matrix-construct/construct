FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-11
ENV CXX clang++-11

ENV packages="\
clang-11 \
llvm-11-dev \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
