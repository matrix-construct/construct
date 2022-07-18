FROM jevolk/construct:ubuntu-22.04-full

ENV CC clang-14
ENV CXX clang++-14

ENV packages="\
clang-14 \
llvm-14-dev \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
