SLIRP4NETNS 1 "July 2018" "Rootless Containers" "User Commands"
==================================================

# NAME

slirp4netns - User-mode networking for unprivileged network namespaces

# SYNOPSIS

slirp4netns [-c] [-e FD] PID TAPNAME

# DESCRIPTION

slirp4netns provides a user-mode networking ("slirp") for unprivileged network namespaces.

Default configuration:

* Gateway: 10.0.2.2
* DNS: 10.0.2.3
* Host: 10.0.2.2, 10.0.2.3

# OPTIONS

**-c**
bring up the interface. IP will be set to 10.0.2.100.

**-e FD**
specify FD for terminating slirp4netns.

# EXAMPLE

Terminal 1:
```console
$ unshare -r -n -m
unshared$ echo $$ > /tmp/pid
unshared$ ip tuntap add name tap0 mode tap
unshared$ ip link set tap0 up
unshared$ ip addr add 10.0.2.100/24 dev tap0
unshared$ ip route add default via 10.0.2.2 dev tap0
unshared$ echo "nameserver 10.0.2.3" > /tmp/resolv.conf
unshared$ mount --bind /tmp/resolv.conf /etc/resolv.conf
```

Terminal 2:
```console
$ slirp4netns $(cat /tmp/pid) tap0
```

Terminal 1:
```console
unshared$ ping 10.0.2.2
unshared$ curl https://example.com
```

# SEE ALSO

**network_namespaces**(7), **user_namespaces**(7)

# AVAILABILITY

The slirp4netns command is available from **https://github.com/rootless-containers/slirp4netns** under GNU GENERAL PUBLIC LICENSE Version 2.
