#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/../tests/common.sh

iperf3 -s > /dev/null &
iperf3_pid=$!

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

mtu=${MTU:=1500}
slirp4netns -c --mtu $mtu $child tun11 &
slirp_pid=$!

wait_for_network_device $child tun11
wait_for_ping_connectivity $child 10.0.2.2

function cleanup {
    kill -9 $iperf3_pid $child $slirp_pid
}
trap cleanup EXIT

nsenter --preserve-credentials -U -n --target=$child iperf3 -c 10.0.2.2 -t "${BENCHMARK_IPERF3_DURATION:-60}"
