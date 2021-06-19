#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

SLIRP_CONFIG_VERSION_MAX=$(slirp4netns -v | grep "SLIRP_CONFIG_VERSION_MAX: " | sed 's#SLIRP_CONFIG_VERSION_MAX: \(\)##')

if [ "${SLIRP_CONFIG_VERSION_MAX:-0}" -lt 2 ]; then
	printf "'--disable-dns' requires SLIRP_CONFIG_VERSION_MAX 2 or newer. Test skipped..."
	exit "$TEST_EXIT_CODE_SKIP"
fi

IPv4_1="127.0.0.1"
IPv4_2=$(ip a | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' | head -n 1)

# For future ipv6 tests
#IPv6_1="::1"
#IPv6_2=$(ip a | sed -En 's/::1\/128//;s/.*inet6 (addr:)?([^ ]*)\/.*$/\2/p' | head -n 1)

function cleanup() {
	rm -rf ncat.log
	kill -9 $child $slirp_pid || exit 0
}
trap cleanup EXIT

port=12122
mtu=${MTU:=1500}

IPs=("$IPv4_1" "$IPv4_2")
for ip in "${IPs[@]}"; do
	ncat -l $port -v >ncat.log 2>&1 &
	ncat1=$!

	unshare -r -n sleep infinity &
	child=$!

	wait_for_network_namespace $child

	slirp4netns -c --mtu $mtu --outbound-addr="$ip" $child tap11 &
	slirp_pid=$!

	wait_for_network_device $child tap11

	wait_for_connectivity $child 10.0.2.2 $port

	wait_process_exits $ncat1
	if ! grep "$ip" ncat.log; then
		printf "%s not found in ncat.log" "$ip"
		exit 1
	fi
	cleanup
	let port=port+1
done
