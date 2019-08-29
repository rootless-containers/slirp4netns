# slirp4netns: User-mode networking for unprivileged network namespaces 

slirp4netns provides user-mode networking ("slirp") for unprivileged network namespaces.

## Motivation

Starting with Linux 3.8, unprivileged users can create [`network_namespaces(7)`](http://man7.org/linux/man-pages/man7/network_namespaces.7.html) along with [`user_namespaces(7)`](http://man7.org/linux/man-pages/man7/user_namespaces.7.html).
However, unprivileged network namespaces had not been very useful, because creating [`veth(4)`](http://man7.org/linux/man-pages/man4/veth.4.html) pairs across the host and network namespaces still requires the root privileges. (i.e. No internet connection)

slirp4netns allows connecting a network namespace to the Internet in a completely unprivileged way, by connecting a TAP device in a network namespace to the usermode TCP/IP stack ("slirp").

## Projects using slirp4netns

Kubernetes distributions:
* [Usernetes](https://github.com/rootless-containers/usernetes) (via RootlessKit)
* [k3s](https://k3s.io) (via RootlessKit)

Container engines:
* [Podman](https://github.com/containers/libpod)
* [Buildah](https://github.com/containers/buildah)
* [ctnr](https://github.com/mgoltzsche/ctnr) (via slirp-cni-plugin)
* [Docker & Moby](https://get.docker.com/rootless) (optionally, via RootlessKit)

Tools:
* [RootlessKit](https://github.com/rootless-containers/rootlesskit)
* [become-root](https://github.com/giuseppe/become-root)
* [slirp-cni-plugin](https://github.com/mgoltzsche/slirp-cni-plugin)

## Maintenance policy

Version                        | Status
-------------------------------|------------------------------------------------------------------------
v0.4.x                         | A :white_check_mark: (will be demoted to B after the release of v0.5.0)
v0.3.x                         | B :white_check_mark: (will be demoted to C after the release of v0.5.0)
v0.2.x                         | C :warning:
Early versions prior to v0.2.x | D :warning:

* A: Actively maintained. Patch releases for security fixes and other bug fixes are planned.
* B: Patch releases for security fixes are planned.
* C: No additional release is planned. However, anybody can still open PR for security fixes in the `release/x.y` branch.
* D: Not maintained. Distributors can continue to distribute this version, but they should apply security fixes by themselves.

See https://github.com/rootless-containers/slirp4netns/releases for the releases.

See https://github.com/rootless-containers/slirp4netns/security/advisories for the past security advisories.

## Quick start

### Install from source

Build dependencies:
* `glib2-devel` (`libglib2.0-dev`)
* `libcap-devel` (`libcap-dev`)
* `libseccomp-devel` (`libseccomp-dev`)

Install steps:

```console
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

* To build `slirp4netns` as a static binary, please run `./configure` with `LDFLAGS=-static`.
* If you set `--prefix` to `$HOME`, you don't need to run `make install` with `sudo`.

### Install from binary

#### RHEL 8 & [Fedora (28 or later)](https://src.fedoraproject.org/rpms/slirp4netns):

```console
$ sudo dnf install slirp4netns
```

#### RHEL/CentOS 7.7

```console
$ sudo yum install slirp4netns
```

#### [RHEL/CentOS 7.6](https://copr.fedorainfracloud.org/coprs/vbatts/shadow-utils-newxidmap/)

```console
$ sudo curl -o /etc/yum.repos.d/vbatts-shadow-utils-newxidmap-epel-7.repo https://copr.fedorainfracloud.org/coprs/vbatts/shadow-utils-newxidmap/repo/epel-7/vbatts-shadow-utils-newxidmap-epel-7.repo
$ sudo yum install slirp4netns
```

You might need to enable user namespaces manually:
```console
$ sudo sh -c 'echo "user.max_user_namespaces=28633" > /etc/sysctl.d/userns.conf'
$ sudo sysctl -p /etc/sysctl.d/userns.conf
```

#### [Arch Linux](https://www.archlinux.org/packages/community/x86_64/slirp4netns/):

```console
$ sudo pacman -S slirp4netns
```

You might need to enable user namespaces manually:
```console
$ sudo sh -c "echo 1 > /proc/sys/kernel/unprivileged_userns_clone"
```

#### [openSUSE Tumbleweed](https://build.opensuse.org/package/show/openSUSE%3AFactory/slirp4netns)

```console
$ sudo zypper install slirp4netns
```

#### [openSUSE Leap 15.0](https://build.opensuse.org/package/show/devel%3Akubic/slirp4netns)

```console
$ sudo zypper addrepo --refresh http://download.opensuse.org/repositories/devel:/kubic/openSUSE_Leap_15.0/devel:kubic.repo
$ sudo zypper install slirp4netns
```

#### [SUSE Linux Enterprise 15](https://build.opensuse.org/package/show/devel%3Akubic/slirp4netns)

```console
$ sudo zypper addrepo --refresh http://download.opensuse.org/repositories/devel:/kubic/SLE_15/devel:kubic.repo
$ sudo zypper install slirp4netns
```

#### [Debian GNU/Linux (10 or later)](https://packages.debian.org/buster/slirp4netns) & [Ubuntu (19.04 or later)](https://packages.ubuntu.com/disco/slirp4netns)

```console
$ sudo apt install slirp4netns
```

#### [NixOS](https://github.com/NixOS/nixpkgs/tree/master/pkgs/tools/networking/slirp4netns)

```console
$ nix-env -i slirp4netns
```

#### [Gentoo Linux](https://packages.gentoo.org/packages/app-emulation/slirp4netns)

```console
$ sudo emerge app-emulation/slirp4netns
```

#### [Slackware](https://git.slackbuilds.org/slackbuilds/tree/network/slirp4netns)

```console
$ sudo sbopkg -i slirp4netns
```

#### [Void Linux](https://github.com/void-linux/void-packages/tree/master/srcpkgs/slirp4netns)

```console
$ sudo xbps-install slirp4netns
```

### Usage

Terminal 1: Create user/network/mount namespaces
```console
$ unshare --user --map-root-user --net --mount
unshared$ echo $$ > /tmp/pid
```

Terminal 2: Start slirp4netns
```console
$ slirp4netns --configure --mtu=65520 --disable-host-loopback $(cat /tmp/pid) tap0
starting slirp, MTU=65520
...
```

Terminal 1: Make sure the `tap0` is configured and connected to the Internet
```console
unshared$ ip a
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
3: tap0: <BROADCAST,UP,LOWER_UP> mtu 65520 qdisc fq_codel state UNKNOWN group default qlen 1000
    link/ether c2:28:0c:0e:29:06 brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.100/24 brd 10.0.2.255 scope global tap0
       valid_lft forever preferred_lft forever
    inet6 fe80::c028:cff:fe0e:2906/64 scope link 
       valid_lft forever preferred_lft forever
unshared$ echo "nameserver 10.0.2.3" > /tmp/resolv.conf
unshared$ mount --bind /tmp/resolv.conf /etc/resolv.conf
unshared$ curl https://example.com
```

See [`slirp4netns.1.md`](slirp4netns.1.md) for further information.

## Benchmarks

### iperf3 (netns -> host)

Aug 28, 2018, on [RootlessKit](https://github.com/rootless-containers/rootlesskit) Travis: https://github.com/rootless-containers/rootlesskit/pull/16

Implementation |  MTU=1500  |  MTU=4000  |  MTU=16384  |  MTU=65520
---------------|------------|------------|-------------|------------
vde_plug       |  763 Mbps  |Unsupported | Unsupported | Unsupported
VPNKit         |  514 Mbps  |  526 Mbps  |   540 Mbps  | Unsupported
slirp4netns    | 1.07 Gbps  | 2.78 Gbps  |  4.55 Gbps  |  9.21 Gbps

slirp4netns is faster than [vde_plug](https://github.com/rd235/vdeplug_slirp) and [VPNKit](https://github.com/moby/vpnkit) because slirp4netns is optimized to avoid copying packets across the namespaces.

The latest revision of slirp4netns is regularly benchmarked (`make benchmark`) on Travis: https://travis-ci.org/rootless-containers/slirp4netns

## Acknowledgement
See [`vendor/README.md`](./vendor/README.md).

## License
[GPL-2.0-or-later](COPYING)
