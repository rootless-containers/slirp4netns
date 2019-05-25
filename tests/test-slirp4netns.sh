#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh


# Test --netns-type=pid
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns $child tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

function cleanup {
    kill -9 $child $slirp_pid
}
trap cleanup EXIT

nsenter --preserve-credentials -U -n --target=$child ip -a netconf | grep tun11
nsenter --preserve-credentials -U -n --target=$child ip addr show tun11 | grep -v inet

kill -9 $child $slirp_pid

# Test --userns-path= --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

slirp4netns --userns-path=/proc/$child/ns/user --netns-type=path /proc/$child/ns/net tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

nsenter --preserve-credentials -U -n --target=$child ip -a netconf | grep tun11
nsenter --preserve-credentials -U -n --target=$child ip addr show tun11 | grep -v inet

kill -9 $child $slirp_pid

# Test --netns-type=path
unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

nsenter --preserve-credentials -U --target=$child slirp4netns --netns-type=path /proc/$child/ns/net tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

nsenter --preserve-credentials -U -n --target=$child ip -a netconf | grep tun11
nsenter --preserve-credentials -U -n --target=$child ip addr show tun11 | grep -v inet
