FROM jevolk/construct:ubuntu-22.04-base

ENV CXX g++-8
ENV CC gcc-8

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y g++-8 gcc-8 \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true
