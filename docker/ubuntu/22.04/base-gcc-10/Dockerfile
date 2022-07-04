FROM jevolk/construct:ubuntu-22.04-base

ENV CXX g++-10
ENV CC gcc-10

RUN true \
    && apt-get update \
    && apt-get install --no-install-recommends -y g++-10 gcc-10 \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && true
