ARG acct
ARG repo
ARG dist_name
ARG dist_version
ARG machine

FROM ${acct}/${repo}:${dist_name}-${dist_version}-base-${machine}

ENV packages="\
libgraphicsmagick-q16-3 \
libpng16-16 \
mesa-opencl-icd \
ocl-icd-opencl-dev \
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
