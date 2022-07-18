FROM jevolk/construct:ubuntu-22.04-base

ENV CC gcc-8
ENV CXX g++-8

ENV packages="\
gcc-8 \
g++-8 \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
