ARG acct
ARG repo
ARG dist_name
ARG dist_version
ARG machine

FROM ${acct}/${repo}:${dist_name}-${dist_version}-base-${machine}

ARG extra_packages

ENV packages="\
libgraphicsmagick-q16-3 \
libpng16-16 \
${extra_packages} \
"

ENV packages_dev="\
${packages_dev} \
libgraphicsmagick1-dev \
libpng-dev \
xxd \
"

RUN true \
&& eval ${do_install} ${packages} \
&& eval ${do_clean} \
&& true
