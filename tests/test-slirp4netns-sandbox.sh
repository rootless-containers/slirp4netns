#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --ready-fd=3 --enable-sandbox $child tap11 3>ready.file &
slirp_pid=$!

# Wait that the sandbox is created
wait_for_file_content 1 ready.file
rm ready.file

# Check there are no capabilities left in slirp4netns
getpcaps $slirp_pid 2>&1 | tail -n1 >slirp.caps
grep cap_net_bind_service slirp.caps
grep -v cap_sys_admin slirp.caps
rm slirp.caps
test -e /proc/$slirp_pid/root/etc
test -e /proc/$slirp_pid/root/run
test \! -e /proc/$slirp_pid/root/home
test \! -e /proc/$slirp_pid/root/root
test \! -e /proc/$slirp_pid/root/var

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

nsenter $(nsenter_flags $child) ip -a netconf | grep tap11
nsenter $(nsenter_flags $child) ip addr show tap11 | grep -v inet
