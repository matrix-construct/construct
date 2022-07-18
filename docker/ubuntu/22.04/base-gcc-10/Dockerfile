FROM jevolk/construct:ubuntu-22.04-base

ENV CC gcc-10
ENV CXX g++-10

ENV packages="\
gcc-10 \
g++-10 \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
