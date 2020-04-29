#!/bin/bash
set -xeuo pipefail

SLIRP_CONFIG_VERSION_MAX=$(slirp4netns -v | grep "SLIRP_CONFIG_VERSION_MAX: " | sed 's#SLIRP_CONFIG_VERSION_MAX: \(\)##')

if [ "${SLIRP_CONFIG_VERSION_MAX:-0}" -lt 3 ]; then
    printf "'--disable-dns' requires SLIRP_CONFIG_VERSION_MAX 3 or newer. Test skipped..."
    exit 0
fi

. $(dirname $0)/common.sh

port=53
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

mtu=${MTU:=1500}
slirp4netns -c --mtu $mtu --disable-dns $child tun11 &
slirp_pid=$!

wait_for_network_device $child tun11
# ping to 10.0.2.2
wait_for_ping_connectivity $child 10.0.2.2

function cleanup() {
    kill -9 $child $slirp_pid
}
trap cleanup EXIT

set +e
err=$(echo "should fail" | nsenter --preserve-credentials -U -n --target=$child ncat -v 10.0.2.3 $port 2>&1)
set -e
echo $err | grep "Connection timed out"
