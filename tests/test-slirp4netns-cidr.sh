#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

set +e
result=$(slirp4netns -c --cidr 24 $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr foo $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr 10.0.2.0 $child tun11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr 10.0.2.100/24 $child tun11 2>&1)
set -e
echo $result | grep "CIDR needs to be a network address like 10.0.2.0/24, not like 10.0.2.100/24"

set +e
result=$(slirp4netns -c --cidr 10.0.2.100/26 $child tun11 2>&1)
set -e
echo $result | grep "prefix length needs to be 1-25"

slirp4netns -c $child --cidr 10.0.135.128/25 tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

function cleanup {
    kill -9 $child $slirp_pid
}
trap cleanup EXIT

result="$(nsenter --preserve-credentials -U -n --target=$child ip a show dev tun11)"
echo "$result" | grep -om1 '^\s*inet .*/' | grep -qF 10.0.135.228
