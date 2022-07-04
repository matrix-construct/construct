FROM jevolk/construct:ubuntu-22.04-base

ENV CXX g++-12
ENV CC gcc-12

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y g++-12 gcc-12 \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true
