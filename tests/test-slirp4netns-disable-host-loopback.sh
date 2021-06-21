#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/../tests/common.sh

port=12121
ncat -l 127.0.0.1 $port &
nc_pid=$!

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

mtu=${MTU:=1500}
slirp4netns -c --mtu $mtu --disable-host-loopback $child tap11 &
slirp_pid=$!

wait_for_network_device $child tap11
# ping to 10.0.2.2 is possible even with --disable-host-loopback
wait_for_ping_connectivity $child 10.0.2.2

function cleanup {
	kill -9 $nc_pid $child $slirp_pid
}
trap cleanup EXIT

set +e
err=$(echo "should fail" | nsenter $(nsenter_flags $child) ncat -v 10.0.2.2 $port 2>&1)
set -e
echo $err | grep "Network is unreachable"
