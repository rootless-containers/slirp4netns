#!/bin/bash
set -xeuo pipefail

. $(dirname $0)/common.sh

unshare -r -n sleep infinity &
child=$!

wait_for_network_namespace $child

tmpdir=$(mktemp -d /tmp/slirp4netns-bench.XXXXXXXXXX)
apisocket=${tmpdir}/slirp4netns.sock
apisocketlongpath=${tmpdir}/slirp4netns-TOO-LONG-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.sock

if slirp4netns -c $child --api-socket $apisocketlongpath tap11; then
	echo "expected failure with apisocket path too long" >&2
	kill -9 $child
	rm -rf $tmpdir
	exit 1
fi

slirp4netns -c $child --api-socket $apisocket tap11 &
slirp_pid=$!

wait_for_network_device $child tap11

function cleanup() {
	kill -9 $child $slirp_pid
	rm -rf $tmpdir
}
trap cleanup EXIT

result=$(echo 'badjson' | ncat -U $apisocket)
echo $result | jq .error.desc | grep "bad request: cannot parse JSON"

result=$(echo '{"unexpectedjson": 42}' | ncat -U $apisocket)
echo $result | jq .error.desc | grep "bad request: no execute found"

result=$(echo '{"execute": "bad"}' | ncat -U $apisocket)
echo $result | jq .error.desc | grep "bad request: unknown execute"

result=$(echo '{"execute": "add_hostfwd", "arguments":{"proto": "bad"}}' | ncat -U $apisocket)
echo $result | jq .error.desc | grep "bad request: add_hostfwd: bad arguments.proto"

set +e
result=$(cat /dev/zero | ncat -U $apisocket || true)
set set -e
echo $result | jq .error.desc | grep "bad request: too large message"

set -e
result=$(echo '{"execute": "add_hostfwd", "arguments":{"proto": "tcp","host_port":8080,"guest_port":80}}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]
id=$(echo $result | jq .return.id)
[[ $id == 1 ]]

result=$(echo '{"execute": "list_hostfwd"}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]
[[ $(echo $result | jq .entries[0].id) == $id ]]
[[ $(echo $result | jq .entries[0].proto) == '"tcp"' ]]
[[ $(echo $result | jq .entries[0].host_addr) == '"0.0.0.0"' ]]
[[ $(echo $result | jq .entries[0].host_port) == 8080 ]]
[[ $(echo $result | jq .entries[0].guest_addr) == '"10.0.2.100"' ]]
[[ $(echo $result | jq .entries[0].guest_port) == 80 ]]

result=$(echo '{"execute": "remove_hostfwd", "arguments":{"id": 1}}' | ncat -U $apisocket)
[[ $(echo $result | jq .error) == null ]]

# see also: benchmarks/benchmark-iperf3-reverse.sh
