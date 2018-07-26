#!/bin/bash

function wait_for_network_namespace {
    # Wait that the namespace is ready.
    COUNTER=0
    while [ $COUNTER -lt 10 ]; do
        if nsenter --preserve-credentials -U -n --target=$1 true; then
            break
        else
            sleep 0.5
        fi
        let COUNTER=COUNTER+1
    done
}

function wait_for_network_device {
    # Wait that the device appears.
    COUNTER=0
    while [ $COUNTER -lt 20 ]; do
        if nsenter --preserve-credentials -U -n --target=$1 ip addr show $2; then
            break
        else
            sleep 0.5
        fi
        let COUNTER=COUNTER+1
    done
}
