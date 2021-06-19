#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

set +e
result=$(slirp4netns -c --cidr 24 $child tap11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr foo $child tap11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr 10.0.2.0 $child tap11 2>&1)
set -e
echo $result | grep "invalid CIDR"

set +e
result=$(slirp4netns -c --cidr 10.0.2.100/24 $child tap11 2>&1)
set -e
echo $result | grep "CIDR needs to be a network address like 10.0.2.0/24, not like 10.0.2.100/24"

set +e
result=$(slirp4netns -c --cidr 10.0.2.100/26 $child tap11 2>&1)
set -e
echo $result | grep "prefix length needs to be 1-25"

slirp4netns -c $child --cidr 10.0.135.128/25 tap11 &
slirp_pid=$!

wait_for_network_device $child tap11

function cleanup {
	kill -9 $child $slirp_pid
}
trap cleanup EXIT

result="$(nsenter $(nsenter_flags $child) ip a show dev tap11)"
echo "$result" | grep -o '^\s*inet .*/' | grep -F 10.0.135.228
