#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns -c $child tap11 &
slirp_pid=$!

wait_for_network_device $child tap11

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

nsenter $(nsenter_flags $child) ip -a netconf | grep tap11

nsenter $(nsenter_flags $child) ip addr show tap11 | grep inet
