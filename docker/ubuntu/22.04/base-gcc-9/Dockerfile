FROM jevolk/construct:ubuntu-22.04-base

ENV CC gcc-9
ENV CXX g++-9

ENV packages="\
gcc-9 \
g++-9 \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
