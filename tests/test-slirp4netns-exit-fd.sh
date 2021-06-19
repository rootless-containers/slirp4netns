#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

touch keep_alive

slirp4netns -e 10 $child tap11 10<(while test -e keep_alive; do sleep 0.1; done) &

slirp_pid=$!

function cleanup {
	set +xeuo pipefail
	kill -9 $child $slirp_pid
	rm -f keep_alive
}
trap cleanup EXIT

# wait a while, check that slirp4netns is alive
kill -0 $slirp_pid

rm keep_alive

wait_process_exits $slirp_pid

if kill -0 $slirp_pid; then
	exit 1
fi

exit 0
