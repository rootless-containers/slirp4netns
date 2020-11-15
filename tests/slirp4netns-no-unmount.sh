#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

# it is a part of test-slirp4netns.sh
# must run in a new mount namespace

mount -t tmpfs tmpfs /run
mkdir /run/foo
mount -t tmpfs tmpfs /run/foo
mount --make-rshared /run

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

./slirp4netns --enable-sandbox --netns-type=path /proc/$child/ns/net tun11 &
slirp_pid=$!

function cleanup {
    kill -9 $child $slirp_pid
}
trap cleanup EXIT

wait_for_network_device $child tun11

findmnt /run/foo
