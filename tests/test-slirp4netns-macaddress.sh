#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh
MACADDRESS="0e:d4:18:d9:38:fb"

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

slirp4netns -c --macaddress $MACADDRESS $child tap11 &
slirp_pid=$!

wait_for_network_device $child tap11

result=$(nsenter $(nsenter_flags $child) ip addr show tap11 | grep -o "ether $MACADDRESS")

if [ -z "$result" ]; then
	printf "expecting %s MAC address on the interface but didn't get it" "$MACADDRESS"
	exit 1
fi

cleanup
