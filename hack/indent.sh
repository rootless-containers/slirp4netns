#!/bin/sh
set -ex

# indent(1): "You must use the ‘-T’ option to tell indent the name of all the typenames in your program that are defined by typedef."
indent -linux -l120 -T ssize_t -T pid_t -T SLIRP *.c
rm -f *.c~
