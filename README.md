# slirp4netns: slirp for network namespaces, without copying buffers across the namespaces

## Install

```console
$ ./make.sh
$ mv slirp4netns ~/bin
```

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

## Acknowledgement

* The files under [`qemu`](./qemu) directory were forked from [QEMU](https://github.com/qemu/qemu/commit/c447afd5783b9237fa51b7a85777007d8d568bfc).
* The files under [`rd235_libslirp`](./rd235_libslirp) directory were forked from [rd235/libslirp](https://github.com/rd235/libslirp/commit/37fd650ad7fba7eb0360b1e1d0abf69cac6eb403).
