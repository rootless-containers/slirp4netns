#!/bin/sh
set -e -x

cc \
    -o slirp4netns \
    -DNDEBUG \
    -Ird235_libslirp/include -Ird235_libslirp/src -Iqemu/slirp \
    qemu/slirp/*.c rd235_libslirp/src/*.c \
    *.c $@
