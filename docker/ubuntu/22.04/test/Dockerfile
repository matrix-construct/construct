ARG acct
ARG repo
ARG feature
ARG cc

FROM ${acct}/${repo}:ubuntu-22.04-${feature}-built-${cc}-${TARGETARCH}

WORKDIR /root
RUN true \
&& construct -debug -smoketest localhost \
&& true
