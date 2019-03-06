FROM ubuntu AS build
RUN apt update && apt install -y automake autotools-dev make gcc libglib2.0-dev
COPY . /slirp4netns
WORKDIR /slirp4netns
RUN chown -R 1000:1000 /slirp4netns
USER 1000:1000
RUN ./autogen.sh && ./configure && make -j $(nproc)

FROM build AS test
USER 0
RUN apt update && apt install -y git indent libtool iproute2 clang clang-tidy iputils-ping iperf3 nmap jq
USER 1000:1000
CMD ["make", "ci"]
