ARG acct
ARG repo

FROM ${acct}/${repo}:ubuntu-22.04-base-${TARGETARCH}

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
