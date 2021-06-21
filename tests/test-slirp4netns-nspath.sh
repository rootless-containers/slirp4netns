#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

# Test --userns-path= --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --userns-path=/proc/$child/ns/user --netns-type=path /proc/$child/ns/net tap11 &
slirp_pid=$!

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

wait_for_network_device $child tap11

nsenter $(nsenter_flags $child) ip -a netconf | grep tap11
nsenter $(nsenter_flags $child) ip addr show tap11 | grep -v inet

kill -9 $child $slirp_pid

# Test --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

nsenter --preserve-credentials -U --target=$child slirp4netns --netns-type=path /proc/$child/ns/net tap11 &
slirp_pid=$!

wait_for_network_device $child tap11

nsenter $(nsenter_flags $child) ip -a netconf | grep tap11
nsenter $(nsenter_flags $child) ip addr show tap11 | grep -v inet
