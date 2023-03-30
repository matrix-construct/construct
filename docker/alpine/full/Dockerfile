ARG acct
ARG repo
ARG dist_name
ARG dist_version
ARG machine

FROM ${acct}/${repo}:${dist_name}-${dist_version}-base-${machine}

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
