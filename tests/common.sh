#!/bin/bash

# See https://www.gnu.org/software/automake/manual/html_node/Scripts_002dbased-Testsuites.html#Testsuite-progress-on-console
TEST_EXIT_CODE_SKIP=77

function nsenter_flags {
	pid=$1
	flags="--target=${pid}"
	userns="$(readlink /proc/${pid}/ns/user)"
	mntns="$(readlink /proc/${pid}/ns/mnt)"
	netns="$(readlink /proc/${pid}/ns/net)"

	self_userns="$(readlink /proc/self/ns/user)"
	self_mntns="$(readlink /proc/self/ns/mnt)"
	self_netns="$(readlink /proc/self/ns/net)"

	if [ "${userns}" != "${self_userns}" ]; then
		flags="$flags --preserve-credentials -U"
	fi
	if [ "${mntns}" != "${self_mntns}" ]; then
		flags="$flags -m"
	fi
	if [ "${netns}" != "${self_netns}" ]; then
		flags="$flags -n"
	fi
	echo "${flags}"
}

function wait_for_network_namespace {
	# Wait that the namespace is ready.
	COUNTER=0
	while [ $COUNTER -lt 40 ]; do
		flags=$(nsenter_flags $1)
		if $(echo $flags | grep -qvw -- -n); then
			flags="$flags -n"
		fi
		if nsenter ${flags} true >/dev/null 2>&1; then
			return 0
		else
			sleep 0.5
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function wait_for_network_device {
	# Wait that the device appears.
	COUNTER=0
	while [ $COUNTER -lt 40 ]; do
		if nsenter $(nsenter_flags $1) ip addr show $2; then
			return 0
		else
			sleep 0.5
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function wait_process_exits {
	COUNTER=0
	while [ $COUNTER -lt 40 ]; do
		if kill -0 $1; then
			sleep 0.5
		else
			return 0
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function wait_for_ping_connectivity {
	COUNTER=0
	while [ $COUNTER -lt 40 ]; do
		if nsenter $(nsenter_flags $1) ping -c 1 -w 1 $2; then
			return 0
		else
			sleep 0.5
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function wait_for_connectivity {
	COUNTER=0
	while [ $COUNTER -lt 40 ]; do
		if echo "wait_for_connectivity" | nsenter $(nsenter_flags $1) ncat -v $2 $3; then
			return 0
		else
			sleep 0.5
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function wait_for_file_content {
	# Wait for a file to get the specified content.
	COUNTER=0
	while [ $COUNTER -lt 20 ]; do
		if grep $1 $2; then
			return 0
		else
			sleep 0.5
		fi
		let COUNTER=COUNTER+1
	done
	exit 1
}

function expose_tcp() {
	apisock=$1 hostport=$2 guestport=$3
	json="{\"execute\": \"add_hostfwd\", \"arguments\": {\"proto\": \"tcp\", \"host_addr\": \"0.0.0.0\", \"host_port\": $hostport, \"guest_addr\": \"10.0.2.100\", \"guest_port\": $guestport}}"
	echo -n $json | ncat -U $apisock
	echo -n "{\"execute\": \"list_hostfwd\"}" | ncat -U $apisock
}
