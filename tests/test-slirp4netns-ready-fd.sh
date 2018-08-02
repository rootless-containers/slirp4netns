#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

touch keep_alive

slirp4netns -c -r 10 $child tun11 10>configured &
slirp_pid=$!

function cleanup {
    set +xeuo pipefail
    kill -9 $child $slirp_pid
    rm -f configured keep_alive
}
trap cleanup EXIT

wait_for_network_device $child tun11

grep 1 configured

exit 0
