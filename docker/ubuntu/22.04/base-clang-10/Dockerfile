FROM jevolk/construct:ubuntu-22.04-base

ENV CC clang-10
ENV CXX clang++-10

ENV packages="\
clang-10 \
llvm-10-dev \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
