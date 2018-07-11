# slirp4netns: slirp for network namespaces, without copying buffers across the namespaces


## Install

Requires [libslirp](https://github.com/rd235/libslirp)(GPL2).

```console
$ autoreconf -if
$ ./configure
$ make
```

Or just `cc -o slirp4netns *.c -lslirp -lpthread`

### Static binary
You can also build a static binary by running `./configure LDFLAGS=-static`.

Note that you need to install libslirp with `./configure --enable-static`.

## Usage

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
