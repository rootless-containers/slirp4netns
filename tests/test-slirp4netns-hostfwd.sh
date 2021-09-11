#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

SLIRP_CONFIG_VERSION_MAX=$(slirp4netns -v | grep "SLIRP_CONFIG_VERSION_MAX: " | sed 's#SLIRP_CONFIG_VERSION_MAX: \(\)##')

if [ "${SLIRP_CONFIG_VERSION_MAX:-0}" -lt 3 ]; then
	printf "forwarding test requires SLIRP_CONFIG_VERSION_MAX 3 or newer. Test skipped..."
	exit "$TEST_EXIT_CODE_SKIP"
fi

if ! ip a | grep inet6 > /dev/null; then
	printf "forwarding test requires ipv6 enabled on host. Test skipped..."
	exit "$TEST_EXIT_CODE_SKIP"
fi

host_port=8080
guest_port=1080
cidr=fd00:a1e1:1724:1a

unshare -r -n socat tcp6-listen:$guest_port,reuseaddr,fork exec:cat,nofork &
child=$!

wait_for_network_namespace $child

tmpdir=$(mktemp -d /tmp/slirp4netns-bench.XXXXXXXXXX)
apisocket=${tmpdir}/slirp4netns.sock

slirp4netns -c $child --enable-ipv6 --cidr6=$cidr::/64 --api-socket $apisocket tun11 &
slirp_pid=$!

wait_for_network_device $child tun11

function cleanup() {
	kill -9 $child $slirp_pid
	rm -rf $tmpdir
}
trap cleanup EXIT

set +e
result=$(cat /dev/zero | ncat -U $apisocket || true)
set set -e
echo $result | jq .error.desc | grep "bad request: too large message"

set -e
result=$(echo '{"execute": "add_hostfwd", "arguments":{"proto": "tcp","host_port":'$host_port',"guest_port":'$guest_port'}}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]
id=$(echo $result | jq .return.id)
[[ $id == 1 ]]

result=$(echo '{"execute": "list_hostfwd"}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]
[[ $(echo $result | jq .entries[0].id) == $id ]]
[[ $(echo $result | jq .entries[0].proto) == '"tcp"' ]]
[[ $(echo $result | jq .entries[0].host_addr) == '"0.0.0.0"' ]]
[[ $(echo $result | jq .entries[0].host_addr6) == '"::"' ]]
[[ $(echo $result | jq .entries[0].host_port) == $host_port ]]
[[ $(echo $result | jq .entries[0].guest_addr) == '"10.0.2.100"' ]]
[[ $(echo $result | jq .entries[0].guest_addr6) == '"'$cidr'::100"' ]]
[[ $(echo $result | jq .entries[0].guest_port) == $guest_port ]]

result=$(echo works | ncat -w 10 -6 localhost $host_port)
[[ "$result" == "works" ]]

result=$(echo works | ncat -w 10 -4 localhost $host_port)
[[ "$result" == "works" ]]

result=$(echo '{"execute": "remove_hostfwd", "arguments":{"id": 1}}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]

# see also: benchmarks/benchmark-iperf3-reverse.sh
