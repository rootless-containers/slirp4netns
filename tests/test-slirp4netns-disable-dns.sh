#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

SLIRP_CONFIG_VERSION_MAX=$(slirp4netns -v | grep "SLIRP_CONFIG_VERSION_MAX: " | sed 's#SLIRP_CONFIG_VERSION_MAX: \(\)##')

if [ "${SLIRP_CONFIG_VERSION_MAX:-0}" -lt 3 ]; then
	printf "'--disable-dns' requires SLIRP_CONFIG_VERSION_MAX 3 or newer. Test skipped..."
	exit "$TEST_EXIT_CODE_SKIP"
fi

port=53
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

mtu=${MTU:=1500}
slirp4netns -c --mtu $mtu --disable-dns $child tap11 &
slirp_pid=$!

wait_for_network_device $child tap11
# ping to 10.0.2.2
wait_for_ping_connectivity $child 10.0.2.2

function cleanup() {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

set +e
err=$(echo "should fail" | nsenter $(nsenter_flags $child) ncat -v 10.0.2.3 $port 2>&1)
set -e
echo $err | grep 'Connection timed out\|TIMEOUT'
