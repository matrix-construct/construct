FROM jevolk/construct:ubuntu-22.04-base

ENV CC gcc-12
ENV CXX g++-12

ENV packages="\
gcc-12 \
g++-12 \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
