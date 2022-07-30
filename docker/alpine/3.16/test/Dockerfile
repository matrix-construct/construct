ARG acct
ARG repo
ARG feature
ARG cc

FROM ${acct}/${repo}:alpine-3.16-${feature}-built-${cc}-${TARGETARCH}

ENV ircd_mods_unload_check=false
WORKDIR /root
RUN true \
&& construct -debug -smoketest localhost \
&& true
