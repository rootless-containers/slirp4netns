#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh


# Test --netns-type=pid
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --ready-fd=3 --enable-sandbox $child tun11 3>ready.file &
slirp_pid=$!

# Wait that the sandbox is created
wait_for_file_content 1 ready.file
rm ready.file

# Check there are no capabilities left in slirp4netns
getpcaps $slirp_pid 2>&1 | tail -n1 > slirp.caps
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

nsenter $(nsenter_flags $child) ip -a netconf | grep tun11
nsenter $(nsenter_flags $child) ip addr show tun11 | grep -v inet

kill -9 $child $slirp_pid

# Test --userns-path= --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --userns-path=/proc/$child/ns/user --netns-type=path /proc/$child/ns/net tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

nsenter $(nsenter_flags $child) ip -a netconf | grep tun11
nsenter $(nsenter_flags $child) ip addr show tun11 | grep -v inet

kill -9 $child $slirp_pid

# Test --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

nsenter --preserve-credentials -U --target=$child slirp4netns --netns-type=path /proc/$child/ns/net tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

nsenter $(nsenter_flags $child) ip -a netconf | grep tun11
nsenter $(nsenter_flags $child) ip addr show tun11 | grep -v inet

unshare -rm $(readlink -f $(dirname $0)/slirp4netns-no-unmount.sh)
