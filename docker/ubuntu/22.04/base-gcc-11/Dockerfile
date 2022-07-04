FROM jevolk/construct:ubuntu-22.04-base

ENV CXX g++-11
ENV CC gcc-11

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y g++-11 gcc-11 \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true
