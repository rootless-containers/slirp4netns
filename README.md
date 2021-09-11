# slirp4netns: User-mode networking for unprivileged network namespaces

slirp4netns provides user-mode networking ("slirp") for unprivileged network namespaces.

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Motivation](#motivation)
- [Projects using slirp4netns](#projects-using-slirp4netns)
- [Maintenance policy](#maintenance-policy)
- [Quick start](#quick-start)
  - [Install](#install)
  - [Usage](#usage)
- [Manual](#manual)
- [Benchmarks](#benchmarks)
  - [iperf3 (netns -> host)](#iperf3-netns---host)
- [Install from source](#install-from-source)
- [Acknowledgement](#acknowledgement)
- [License](#license)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## Motivation

Starting with Linux 3.8, unprivileged users can create [`network_namespaces(7)`](http://man7.org/linux/man-pages/man7/network_namespaces.7.html) along with [`user_namespaces(7)`](http://man7.org/linux/man-pages/man7/user_namespaces.7.html).
However, unprivileged network namespaces had not been very useful, because creating [`veth(4)`](http://man7.org/linux/man-pages/man4/veth.4.html) pairs across the host and network namespaces still requires the root privileges. (i.e. No internet connection)

slirp4netns allows connecting a network namespace to the Internet in a completely unprivileged way, by connecting a TAP device in a network namespace to the usermode TCP/IP stack (["slirp"](https://gitlab.freedesktop.org/slirp/libslirp)).

## Projects using slirp4netns

Kubernetes distributions:
* [Usernetes](https://github.com/rootless-containers/usernetes) (via RootlessKit)
* [k3s](https://k3s.io) (via RootlessKit)

Container engines:
* [Podman](https://github.com/containers/libpod)
* [Buildah](https://github.com/containers/buildah)
* [ctnr](https://github.com/mgoltzsche/ctnr) (via slirp-cni-plugin)
* [Docker & Moby](https://get.docker.com/rootless) (optionally, via RootlessKit)
* [containerd/nerdctl](https://github.com/containerd/nerdctl) (optionally, via RootlessKit)

Tools:
* [RootlessKit](https://github.com/rootless-containers/rootlesskit)
* [become-root](https://github.com/giuseppe/become-root)
* [slirp-cni-plugin](https://github.com/mgoltzsche/slirp-cni-plugin)

## Maintenance policy

Version                        | Status
-------------------------------|------------------------------------------------------------------------
v1.1.x                         | :white_check_mark: Active
v1.0.x                         | End of Life (Jun  2, 2020)
v0.4.x                         | End of Life (Sep 30, 2020)
v0.3.x                         | End of Life (Mar 31, 2020)
v0.2.x                         | End of Life (Aug 30, 2019)
Early versions prior to v0.2.x | End of Life (Jan  5, 2019)

See https://github.com/rootless-containers/slirp4netns/releases for the releases.

See https://github.com/rootless-containers/slirp4netns/security/advisories for the past security advisories.

## Quick start

### Install

Statically linked binaries available for x86\_64, aarch64, armv7l, s390x, and ppc64le: https://github.com/rootless-containers/slirp4netns/releases

Also available as a package on almost all Linux distributions:
* [RHEL/CentOS (since 7.7 and 8.0)](https://pkgs.org/search/?q=slirp4netns)
* [Fedora (since 28)](https://src.fedoraproject.org/rpms/slirp4netns)
* [Arch Linux](https://www.archlinux.org/packages/community/x86_64/slirp4netns/)
* [openSUSE (since Leap 15.0)](https://build.opensuse.org/package/show/openSUSE%3AFactory/slirp4netns)
* [SUSE Linux Enterprise (since 15)](https://build.opensuse.org/package/show/devel%3Akubic/slirp4netns)
* [Debian GNU/Linux (since 10.0)](https://packages.debian.org/buster/slirp4netns)
* [Ubuntu (since 19.04)](https://packages.ubuntu.com/search?keywords=slirp4netns)
* [NixOS](https://github.com/NixOS/nixpkgs/tree/master/pkgs/tools/networking/slirp4netns)
* [Gentoo Linux](https://packages.gentoo.org/packages/app-emulation/slirp4netns)
* [Slackware](https://git.slackbuilds.org/slackbuilds/tree/network/slirp4netns)
* [Void Linux](https://github.com/void-linux/void-packages/tree/master/srcpkgs/slirp4netns)
* [Alpine Linux (since 3.14)](https://pkgs.alpinelinux.org/packages?name=slirp4netns)

e.g.

```console
$ sudo apt-get install slirp4netns
```

To install slirp4netns from the source, see [Install from source](#install-from-source).

### Usage

**Terminal 1**: Create user/network/mount namespaces

```console
(host)$ unshare --user --map-root-user --net --mount
(namespace)$ echo $$ > /tmp/pid
```

In this documentation, we use `(host)$` as the prompt of the host shell, `(namespace)$` as the prompt of the shell running in the namespaces.

If `unshare` fails, try the following commands (known to be needed on Debian, Arch, and old CentOS 7.X):

```console
(host)$ sudo sh -c 'echo "user.max_user_namespaces=28633" >> /etc/sysctl.d/userns.conf'
(host)$ [ -f /proc/sys/kernel/unprivileged_userns_clone ] && sudo sh -c 'echo "kernel.unprivileged_userns_clone=1" >> /etc/sysctl.d/userns.conf'
(host)$ sudo sysctl --system
```

**Terminal 2**: Start slirp4netns

```console
(host)$ slirp4netns --configure --mtu=65520 --disable-host-loopback $(cat /tmp/pid) tap0
starting slirp, MTU=65520
...
```

**Terminal 1**: Make sure the `tap0` is configured and connected to the Internet

```console
(namespace)$ ip a
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
3: tap0: <BROADCAST,UP,LOWER_UP> mtu 65520 qdisc fq_codel state UNKNOWN group default qlen 1000
    link/ether c2:28:0c:0e:29:06 brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.100/24 brd 10.0.2.255 scope global tap0
       valid_lft forever preferred_lft forever
    inet6 fe80::c028:cff:fe0e:2906/64 scope link
       valid_lft forever preferred_lft forever
(namespace)$ echo "nameserver 10.0.2.3" > /tmp/resolv.conf
(namespace)$ mount --bind /tmp/resolv.conf /etc/resolv.conf
(namespace)$ curl https://example.com
```

## Manual

Manual: [`slirp4netns.1.md`](slirp4netns.1.md)

* [Description](./slirp4netns.1.md#description)
* [Options](./slirp4netns.1.md#options)
* [Example](./slirp4netns.1.md#example)
* [Routing ping packets](./slirp4netns.1.md#routing-ping-packets)
* [API socket](./slirp4netns.1.md#api-socket)
* [Defined namespace paths](./slirp4netns.1.md#defined-namespace-paths)
* [Outbound addresses](./slirp4netns.1.md#outbound-addresses)
* [Inter-namespace communication](./slirp4netns.1.md#inter-namespace-communication)
* [Inter-host communication](./slirp4netns.1.md#inter-host-communication)
* [Bugs](./slirp4netns.1.md#bugs)

## Benchmarks

### iperf3 (netns -> host)

Aug 28, 2018, on [RootlessKit](https://github.com/rootless-containers/rootlesskit) Travis: https://github.com/rootless-containers/rootlesskit/pull/16

Implementation |  MTU=1500  |  MTU=4000  |  MTU=16384  |  MTU=65520
---------------|------------|------------|-------------|------------
vde_plug       |  763 Mbps  |Unsupported | Unsupported | Unsupported
VPNKit         |  514 Mbps  |  526 Mbps  |   540 Mbps  | Unsupported
slirp4netns    | 1.07 Gbps  | 2.78 Gbps  |  4.55 Gbps  |  9.21 Gbps

slirp4netns is faster than [vde_plug](https://github.com/rd235/vdeplug_slirp) and [VPNKit](https://github.com/moby/vpnkit) because slirp4netns is optimized to avoid copying packets across the namespaces.

The latest revision of slirp4netns is regularly benchmarked (`make benchmark`) on [CI](https://github.com/rootless-containers/slirp4netns/actions?query=workflow%3AMain).

## Install from source

Build dependencies (`apt-get`):

```console
$ sudo apt-get install libglib2.0-dev libslirp-dev libcap-dev libseccomp-dev
```

Build dependencies (`dnf`):

```console
$ sudo dnf install glib2-devel libslirp-devel libcap-devel libseccomp-devel
```

Installation steps:

```console
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

* [libslirp](https://gitlab.freedesktop.org/slirp/libslirp) needs to be v4.4.0.57 or later.
* To build `slirp4netns` as a static binary, run `./configure` with `LDFLAGS=-static`.
* If you set `--prefix` to `$HOME`, you don't need to run `make install` with `sudo`.

## Acknowledgement
See [`vendor/README.md`](./vendor/README.md).

## License
[GPL-2.0-or-later](COPYING)
