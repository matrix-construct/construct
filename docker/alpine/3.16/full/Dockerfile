FROM jevolk/construct:alpine-3.16-base

ENV packages="\
freetype \
graphicsmagick \
libpng \
"

ENV packages_dev="\
${packages_dev} \
freetype-dev \
graphicsmagick-dev \
libpng-dev \
"

RUN true \
&& apk add --no-cache ${packages} \
&& true
