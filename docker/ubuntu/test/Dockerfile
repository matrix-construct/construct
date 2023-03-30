ARG acct
ARG repo
ARG dist_name
ARG dist_version
ARG feature
ARG cc
ARG machine

FROM ${acct}/${repo}:${dist_name}-${dist_version}-${feature}-built-${cc}-${machine}

WORKDIR /root
RUN true \
&& construct -debug -smoketest localhost \
&& true
