#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

set +e
result=$(slirp4netns -c --enable-ipv6 --cidr6 64 $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --enable-ipv6 --cidr6 foo $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --enable-ipv6 --cidr6 fd00::2 $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --enable-ipv6 --cidr6 fd00::/129 $child tun11 2>&1)
set -e
echo $result | grep "prefix length needs to be 8-128"

cidr=fd00:a1e1:1724:1a
slirp4netns -c $child --enable-ipv6 --cidr6 $cidr::/64 tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

function cleanup {
    kill -9 $child $slirp_pid
}
trap cleanup EXIT

result="$(nsenter --preserve-credentials -U -n --target=$child ip a show dev tun11)"
echo "$result" | grep -o '^\s*inet6 .*/..' | grep -F $cidr::100/64

cleanup
sleep 1

unshare -r -n sleep infinity &
child=$!

slirp4netns -c $child --enable-ipv6 tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

result="$(nsenter --preserve-credentials -U -n --target=$child ip a show dev tun11)"
echo "$result" | grep -o '^\s*inet6 .*/..' | grep -F fd00::100/64

cleanup
sleep 1

unshare -r -n sleep infinity &
child=$!

slirp4netns -c $child --enable-ipv6 --ipv6-random tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

result="$(nsenter --preserve-credentials -U -n --target=$child ip a show dev tun11)"
echo "$result" | grep -o '^\s*inet6 .*/..' | grep -vF fd00::100/64
