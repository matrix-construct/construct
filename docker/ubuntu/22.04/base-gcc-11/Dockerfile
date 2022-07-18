FROM jevolk/construct:ubuntu-22.04-base

ENV CC gcc-11
ENV CXX g++-11

ENV packages="\
gcc-11 \
g++-11 \
"

RUN true \
&& apt-get update \
&& apt-get install --no-install-recommends -y ${packages} \
&& apt-get clean -y \
&& rm -rf /var/lib/apt/lists/* \
&& true
