#!/bin/bash
set -xeuo pipefail

if [[ ! -v "CHILD" ]]; then
	# reexec in a new mount namespace
	export CHILD=1
	exec unshare -rm "$0" "$@"
fi

. $(dirname $0)/common.sh

mount -t tmpfs tmpfs /run
mkdir /run/foo
mount -t tmpfs tmpfs /run/foo
mount --make-rshared /run

unshare -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --enable-sandbox --netns-type=path /proc/$child/ns/net tap11 &
slirp_pid=$!

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

wait_for_network_device $child tap11

findmnt /run/foo
