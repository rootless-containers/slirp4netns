FROM alpine:3.8 AS buildtest-alpine38-static
RUN apk add --no-cache git build-base autoconf automake libtool linux-headers glib-dev glib-static
COPY . /src
WORKDIR /src
RUN ./autogen.sh && ./configure LDFLAGS="-static" && make && cp -f slirp4netns /

FROM ubuntu:18.04 AS buildtest-ubuntu1804-common
RUN apt update && apt install -y automake autotools-dev make gcc libglib2.0-dev
COPY . /src
WORKDIR /src
RUN ./autogen.sh

FROM buildtest-ubuntu1804-common AS buildtest-ubuntu1804-dynamic
RUN ./configure && make && cp -f slirp4netns /

FROM buildtest-ubuntu1804-common AS buildtest-ubuntu1804-static
RUN ./configure LDFLAGS="-static" && make && cp -f slirp4netns /

FROM scratch AS buildtest-final-stage
COPY --from=buildtest-alpine38-static /slirp4netns /buildtest-alpine38-static
COPY --from=buildtest-ubuntu1804-dynamic /slirp4netns /buildtest-ubuntu1804-dynamic
COPY --from=buildtest-ubuntu1804-static /slirp4netns /buildtest-ubuntu1804-static
