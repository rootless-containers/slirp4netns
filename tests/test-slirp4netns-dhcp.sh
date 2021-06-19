#!/bin/bash

# This test should pass with libslirp < 4.6.0 and libslirp >= 4.6.1,
# but should fail with libslirp == 4.6.0 .
# https://gitlab.freedesktop.org/slirp/libslirp/-/issues/48

set -xeuo pipefail

. $(dirname $0)/common.sh

if ! command -v udhcpc 2>&1; then
	echo "udhcpc is missing, skipping the test"
	exit "$TEST_EXIT_CODE_SKIP"
fi

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns $child tap0 &
slirp_pid=$!

udhcpc_log=$(mktemp /tmp/slirp4netns-test-udhcpc.XXXXXXXXXX)

function cleanup {
	kill -9 $child $slirp_pid
	rm -f $udhcpc_log
}
trap cleanup EXIT

nsenter $(nsenter_flags $child) ip link set lo up
nsenter $(nsenter_flags $child) ip link set tap0 up
nsenter $(nsenter_flags $child) timeout 10s udhcpc -q -i tap0 -s /bin/true 2>&1 | tee $udhcpc_log
grep 10.0.2.15 $udhcpc_log
