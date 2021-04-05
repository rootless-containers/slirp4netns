/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <getopt.h>
#include <stdbool.h>
#include <regex.h>
#include <libslirp.h>
#include "slirp4netns.h"
#include <ifaddrs.h>
#include <seccomp.h>

#define DEFAULT_MTU (1500)
#define DEFAULT_CIDR ("10.0.2.0/24")
#define DEFAULT_VHOST_OFFSET (2) // 10.0.2.2
#define DEFAULT_VDHCPSTART_OFFSET (15) // 10.0.2.15
#define DEFAULT_VNAMESERVER_OFFSET (3) // 10.0.2.3
#define DEFAULT_RECOMMENDED_VGUEST_OFFSET (100) // 10.0.2.100
#define DEFAULT_NETNS_TYPE ("pid")
#define NETWORK_PREFIX_MIN (1)
// >=26 is not supported because the recommended guest IP is set to network addr
// + 100 .
#define NETWORK_PREFIX_MAX (25)

static int nsenter(pid_t target_pid, char *netns, char *userns,
                   bool only_userns)
{
    int usernsfd = -1, netnsfd = -1;
    if (!only_userns && !netns) {
        if (asprintf(&netns, "/proc/%d/ns/net", target_pid) < 0) {
            perror("cannot get netns path");
            return -1;
        }
    }
    if (!userns && target_pid) {
        if (asprintf(&userns, "/proc/%d/ns/user", target_pid) < 0) {
            perror("cannot get userns path");
            return -1;
        }
    }
    if (!only_userns && (netnsfd = open(netns, O_RDONLY)) < 0) {
        perror(netns);
        return netnsfd;
    }
    if (userns && (usernsfd = open(userns, O_RDONLY)) < 0) {
        perror(userns);
        return usernsfd;
    }

    if (usernsfd != -1) {
        int r = setns(usernsfd, CLONE_NEWUSER);
        if (only_userns && r < 0) {
            perror("setns(CLONE_NEWUSER)");
            return -1;
        }
        close(usernsfd);
    }
    if (netnsfd != -1 && setns(netnsfd, CLONE_NEWNET) < 0) {
        perror("setns(CLONE_NEWNET)");
        return -1;
    }
    close(netnsfd);
    return 0;
}

static int open_tap(const char *tapname)
{
    int fd;
    struct ifreq ifr;
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("open(\"/dev/net/tun\")");
        return fd;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, tapname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return -1;
    }
    return fd;
}

static int sendfd(int sock, int fd)
{
    ssize_t rc;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(fd))];
    struct iovec iov;
    char dummy = '\0';
    memset(&msg, 0, sizeof(msg));
    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
    msg.msg_controllen = cmsg->cmsg_len;
    if ((rc = sendmsg(sock, &msg, 0)) < 0) {
        perror("sendmsg");
    }
    return rc;
}

static int configure_network(const char *tapname,
                             struct slirp4netns_config *cfg)
{
    struct rtentry route;
    struct ifreq ifr;
    struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("cannot create socket");
        return -1;
    }

    // set loopback device to UP
    struct ifreq ifr_lo = { .ifr_name = "lo",
                            .ifr_flags = IFF_UP | IFF_RUNNING };
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr_lo) < 0) {
        perror("cannot set device up");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_UP | IFF_RUNNING;
    strncpy(ifr.ifr_name, tapname, sizeof(ifr.ifr_name) - 1);

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("cannot set device up");
        return -1;
    }

    ifr.ifr_mtu = (int)cfg->mtu;
    if (ioctl(sockfd, SIOCSIFMTU, &ifr) < 0) {
        perror("cannot set MTU");
        return -1;
    }

    if (cfg->vmacaddress_len > 0) {
        ifr.ifr_ifru.ifru_hwaddr = cfg->vmacaddress;
        if (ioctl(sockfd, SIOCSIFHWADDR, &ifr) < 0) {
            perror("cannot set MAC address");
            return -1;
        }
    }

    sai->sin_family = AF_INET;
    sai->sin_port = 0;
    sai->sin_addr = cfg->recommended_vguest;

    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
        perror("cannot set device address");
        return -1;
    }

    sai->sin_addr = cfg->vnetmask;
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
        perror("cannot set device netmask");
        return -1;
    }

    memset(&route, 0, sizeof(route));
    sai = (struct sockaddr_in *)&route.rt_gateway;
    sai->sin_family = AF_INET;
    sai->sin_addr = cfg->vhost;
    sai = (struct sockaddr_in *)&route.rt_dst;
    sai->sin_family = AF_INET;
    sai->sin_addr.s_addr = INADDR_ANY;
    sai = (struct sockaddr_in *)&route.rt_genmask;
    sai->sin_family = AF_INET;
    sai->sin_addr.s_addr = INADDR_ANY;

    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_metric = 0;
    route.rt_dev = (char *)tapname;

    if (ioctl(sockfd, SIOCADDRT, &route) < 0) {
        perror("set route");
        return -1;
    }
    return 0;
}

struct in6_ifreq {
    struct in6_addr ifr6_addr;
    __u32 ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};

static int configure_network6(const char *tapname,
                              struct slirp4netns_config *cfg)
{
    struct rtentry route;
    struct ifreq ifr;
    struct in6_ifreq ifr6;
    struct sockaddr_in6 sai;
    int sockfd;

    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("cannot create socket");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_UP | IFF_RUNNING;
    strncpy(ifr.ifr_name, tapname, sizeof(ifr.ifr_name) - 1);

    if (ioctl(sockfd, SIOGIFINDEX, &ifr) < 0) {
        perror("cannot get dev index");
        return -1;
    }

    memset(&sai, 0, sizeof(struct sockaddr));
    sai.sin6_family = AF_INET6;
    sai.sin6_port = 0;

    if (inet_pton(AF_INET6, "fd00::100", &sai.sin6_addr) != 1) {
        perror("cannot create device address");
        return -1;
    }

    memcpy((char *)&ifr6.ifr6_addr, &sai.sin6_addr, sizeof(struct in6_addr));

    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 64;

    if (ioctl(sockfd, SIOCSIFADDR, &ifr6) < 0) {
        perror("cannot set device address");
        return -1;
    }

    return 0;
}

static int child(int sock, pid_t target_pid, bool do_config_network,
                 const char *tapname, char *netns_path, char *userns_path,
                 struct slirp4netns_config *cfg)
{
    int rc, tapfd;
    if ((rc = nsenter(target_pid, netns_path, userns_path, false)) < 0) {
        return rc;
    }
    if ((tapfd = open_tap(tapname)) < 0) {
        return tapfd;
    }
    if (do_config_network && configure_network(tapname, cfg) < 0) {
        return -1;
    }
    if (do_config_network && configure_network6(tapname, cfg) < 0) {
        return -1;
    }
    if (sendfd(sock, tapfd) < 0) {
        close(tapfd);
        close(sock);
        return -1;
    }
    fprintf(stderr, "sent tapfd=%d for %s\n", tapfd, tapname);
    close(sock);
    return 0;
}

static int recvfd(int sock)
{
    int fd;
    ssize_t rc;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(fd))];
    struct iovec iov;
    char dummy = '\0';
    memset(&msg, 0, sizeof(msg));
    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    if ((rc = recvmsg(sock, &msg, 0)) < 0) {
        perror("recvmsg");
        return (int)rc;
    }
    if (rc == 0) {
        fprintf(stderr, "the message is empty\n");
        return -1;
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "the message does not contain fd\n");
        return -1;
    }
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
    return fd;
}

static int parent(int sock, int ready_fd, int exit_fd, const char *api_socket,
                  struct slirp4netns_config *cfg, pid_t target_pid)
{
    int rc, tapfd;
    if ((tapfd = recvfd(sock)) < 0) {
        return tapfd;
    }
    fprintf(stderr, "received tapfd=%d\n", tapfd);
    close(sock);
    printf("Starting slirp\n");
    printf("* MTU:             %d\n", cfg->mtu);
    printf("* Network:         %s\n", inet_ntoa(cfg->vnetwork));
    printf("* Netmask:         %s\n", inet_ntoa(cfg->vnetmask));
    printf("* Gateway:         %s\n", inet_ntoa(cfg->vhost));
    printf("* DNS:             %s\n", inet_ntoa(cfg->vnameserver));
    printf("* Recommended IP:  %s\n", inet_ntoa(cfg->recommended_vguest));
    if (api_socket != NULL) {
        printf("* API Socket:      %s\n", api_socket);
    }
#if SLIRP_CONFIG_VERSION_MAX >= 2
    if (cfg->enable_outbound_addr) {
        printf("* Outbound IPv4:    %s\n",
               inet_ntoa(cfg->outbound_addr.sin_addr));
    }
    if (cfg->enable_outbound_addr6) {
        char str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &cfg->outbound_addr6.sin6_addr, str,
                      INET6_ADDRSTRLEN) != NULL) {
            printf("* Outbound IPv6:    %s\n", str);
        }
    }
#endif
    if (cfg->vmacaddress_len > 0) {
        unsigned char *mac = (unsigned char *)cfg->vmacaddress.sa_data;
        printf("* MAC address:     %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0],
               mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    if (!cfg->disable_host_loopback) {
        printf(
            "WARNING: 127.0.0.1:* on the host is accessible as %s (set "
            "--disable-host-loopback to prohibit connecting to 127.0.0.1:*)\n",
            inet_ntoa(cfg->vhost));
    }
    if (cfg->enable_sandbox && geteuid() != 0) {
        if ((rc = nsenter(target_pid, NULL, NULL, true)) < 0) {
            close(tapfd);
            return rc;
        }
        if ((rc = setegid(0)) < 0) {
            fprintf(stderr, "setegid(0)\n");
            close(tapfd);
            return rc;
        }
        if ((rc = seteuid(0)) < 0) {
            fprintf(stderr, "seteuid(0)\n");
            close(tapfd);
            return rc;
        }
    }
    if ((rc = do_slirp(tapfd, ready_fd, exit_fd, api_socket, cfg)) < 0) {
        fprintf(stderr, "do_slirp failed\n");
        close(tapfd);
        return rc;
    }
    /* NOT REACHED */
    return 0;
}

static void usage(const char *argv0)
{
    printf("Usage: %s [OPTION]... PID|PATH TAPNAME\n", argv0);
    printf("User-mode networking for unprivileged network namespaces.\n\n");
    printf("-c, --configure          bring up the interface\n");
    printf("-e, --exit-fd=FD         specify the FD for terminating "
           "slirp4netns\n");
    printf("-r, --ready-fd=FD        specify the FD to write to when the "
           "network is configured\n");
    /* v0.2.0 */
    printf("-m, --mtu=MTU            specify MTU (default=%d, max=65521)\n",
           DEFAULT_MTU);
    printf("-6, --enable-ipv6        enable IPv6 (experimental)\n");
    /* v0.3.0 */
    printf("-a, --api-socket=PATH    specify API socket path\n");
    printf(
        "--cidr=CIDR              specify network address CIDR (default=%s)\n",
        DEFAULT_CIDR);
    printf("--disable-host-loopback  prohibit connecting to 127.0.0.1:* on the "
           "host namespace\n");
    /* v0.4.0 */
    printf("--netns-type=TYPE 	 specify network namespace type ([path|pid], "
           "default=%s)\n",
           DEFAULT_NETNS_TYPE);
    printf("--userns-path=PATH	 specify user namespace path\n");
    printf(
        "--enable-sandbox         create a new mount namespace (and drop all "
        "caps except CAP_NET_BIND_SERVICE if running as the root)\n");
    printf("--enable-seccomp         enable seccomp to limit syscalls "
           "(experimental)\n");
    /* v1.1.0 */
#if SLIRP_CONFIG_VERSION_MAX >= 2
    printf("--outbound-addr=IPv4     sets outbound ipv4 address to bound to "
           "(experimental)\n");
    printf("--outbound-addr6=IPv6    sets outbound ipv6 address to bound to "
           "(experimental)\n");
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 3
    printf("--disable-dns            disables 10.0.2.3 (or configured internal "
           "ip) to host dns redirect (experimental)\n");
#endif
    printf("--macaddress=MAC         specify the MAC address of the TAP (only "
           "valid with -c)\n");
    /* others */
    printf("-h, --help               show this help and exit\n");
    printf("-v, --version            show version and exit\n");
}

// version output is runc-compatible and machine-parsable
static void version()
{
    const struct scmp_version *scmpv = seccomp_version();
    printf("slirp4netns version %s\n", VERSION ? VERSION : PACKAGE_VERSION);
#ifdef COMMIT
    printf("commit: %s\n", COMMIT);
#endif
    printf("libslirp: %s\n", slirp_version_string());
    printf("SLIRP_CONFIG_VERSION_MAX: %d\n", SLIRP_CONFIG_VERSION_MAX);
    if (scmpv != NULL) {
        printf("libseccomp: %d.%d.%d\n", scmpv->major, scmpv->minor,
               scmpv->micro);
        /* Do not free scmpv */
    }
}

struct options {
    char *tapname; // argv[2]
    char *cidr; // --cidr
    char *api_socket; // -a
    char *netns_type; // argv[1]
    char *netns_path; // --netns-path
    char *userns_path; // --userns-path
    char *outbound_addr; // --outbound-addr
    char *outbound_addr6; // --outbound-addr6
    pid_t target_pid; // argv[1]
    int exit_fd; // -e
    int ready_fd; // -r
    unsigned int mtu; // -m
    bool do_config_network; // -c
    bool disable_host_loopback; // --disable-host-loopback
    bool enable_ipv6; // -6
    bool enable_sandbox; // --enable-sandbox
    bool enable_seccomp; // --enable-seccomp
    bool disable_dns; // --disable-dns
    char *macaddress; // --macaddress
};

static void options_init(struct options *options)
{
    memset(options, 0, sizeof(*options));
    options->exit_fd = options->ready_fd = -1;
    options->mtu = DEFAULT_MTU;
}

static void options_destroy(struct options *options)
{
    if (options->tapname != NULL) {
        free(options->tapname);
        options->tapname = NULL;
    }
    if (options->cidr != NULL) {
        free(options->cidr);
        options->cidr = NULL;
    }
    if (options->api_socket != NULL) {
        free(options->api_socket);
        options->api_socket = NULL;
    }
    if (options->netns_type != NULL) {
        free(options->netns_type);
        options->netns_type = NULL;
    }
    if (options->netns_path != NULL) {
        free(options->netns_path);
        options->netns_path = NULL;
    }
    if (options->userns_path != NULL) {
        free(options->userns_path);
        options->userns_path = NULL;
    }
    if (options->outbound_addr != NULL) {
        free(options->outbound_addr);
        options->outbound_addr = NULL;
    }
    if (options->outbound_addr6 != NULL) {
        free(options->outbound_addr6);
        options->outbound_addr6 = NULL;
    }
    if (options->macaddress != NULL) {
        free(options->macaddress);
        options->macaddress = NULL;
    }
}

// * caller does not need to call options_init()
// * caller needs to call options_destroy() after calling this function.
// * this function calls exit() on an error.
static void parse_args(int argc, char *const argv[], struct options *options)
{
    int opt;
    char *strtol_e = NULL;
    char *optarg_cidr = NULL;
    char *optarg_netns_type = NULL;
    char *optarg_userns_path = NULL;
    char *optarg_api_socket = NULL;
    char *optarg_outbound_addr = NULL;
    char *optarg_outbound_addr6 = NULL;
    char *optarg_macaddress = NULL;
#define CIDR -42
#define DISABLE_HOST_LOOPBACK -43
#define NETNS_TYPE -44
#define USERNS_PATH -45
#define ENABLE_SANDBOX -46
#define ENABLE_SECCOMP -47
#define OUTBOUND_ADDR -48
#define OUTBOUND_ADDR6 -49
#define DISABLE_DNS -50
#define MACADDRESS -51
#define _DEPRECATED_NO_HOST_LOOPBACK \
    -10043 // deprecated in favor of disable-host-loopback
#define _DEPRECATED_CREATE_SANDBOX \
    -10044 // deprecated in favor of enable-sandbox
    const struct option longopts[] = {
        { "configure", no_argument, NULL, 'c' },
        { "exit-fd", required_argument, NULL, 'e' },
        { "ready-fd", required_argument, NULL, 'r' },
        { "mtu", required_argument, NULL, 'm' },
        { "cidr", required_argument, NULL, CIDR },
        { "disable-host-loopback", no_argument, NULL, DISABLE_HOST_LOOPBACK },
        { "no-host-loopback", no_argument, NULL, _DEPRECATED_NO_HOST_LOOPBACK },
        { "netns-type", required_argument, NULL, NETNS_TYPE },
        { "userns-path", required_argument, NULL, USERNS_PATH },
        { "api-socket", required_argument, NULL, 'a' },
        { "enable-ipv6", no_argument, NULL, '6' },
        { "enable-sandbox", no_argument, NULL, ENABLE_SANDBOX },
        { "create-sandbox", no_argument, NULL, _DEPRECATED_CREATE_SANDBOX },
        { "enable-seccomp", no_argument, NULL, ENABLE_SECCOMP },
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'v' },
        { "outbound-addr", required_argument, NULL, OUTBOUND_ADDR },
        { "outbound-addr6", required_argument, NULL, OUTBOUND_ADDR6 },
        { "disable-dns", no_argument, NULL, DISABLE_DNS },
        { "macaddress", required_argument, NULL, MACADDRESS },
        { 0, 0, 0, 0 },
    };
    options_init(options);
    /* NOTE: clang-tidy hates strdup(optarg) in the while loop (#112) */
    while ((opt = getopt_long(argc, argv, "ce:r:m:a:6hv", longopts, NULL)) !=
           -1) {
        switch (opt) {
        case 'c':
            options->do_config_network = true;
            break;
        case 'e':
            errno = 0;
            options->exit_fd = strtol(optarg, &strtol_e, 10);
            if (errno || *strtol_e != '\0' || options->exit_fd < 0) {
                fprintf(stderr, "exit-fd must be a non-negative integer\n");
                goto error;
            }
            break;
        case 'r':
            errno = 0;
            options->ready_fd = strtol(optarg, &strtol_e, 10);
            if (errno || *strtol_e != '\0' || options->ready_fd < 0) {
                fprintf(stderr, "ready-fd must be a non-negative integer\n");
                goto error;
            }
            break;
        case 'm':
            errno = 0;
            options->mtu = strtol(optarg, &strtol_e, 10);
            if (errno || *strtol_e != '\0' || options->mtu <= 0 ||
                options->mtu > 65521) {
                fprintf(stderr, "MTU must be a positive integer (< 65522)\n");
                goto error;
            }
            break;
        case CIDR:
            optarg_cidr = optarg;
            break;
        case _DEPRECATED_NO_HOST_LOOPBACK:
            // There was no tagged release with support for --no-host-loopback.
            // So no one will be affected by removal of --no-host-loopback.
            printf("WARNING: --no-host-loopback is deprecated and will be "
                   "removed in future releases, please use "
                   "--disable-host-loopback instead.\n");
            /* FALLTHROUGH */
        case DISABLE_HOST_LOOPBACK:
            options->disable_host_loopback = true;
            break;
        case _DEPRECATED_CREATE_SANDBOX:
            // There was no tagged release with support for --create-sandbox.
            // So no one will be affected by removal of --create-sandbox.
            printf("WARNING: --create-sandbox is deprecated and will be "
                   "removed in future releases, please use "
                   "--enable-sandbox instead.\n");
            /* FALLTHROUGH */
        case ENABLE_SANDBOX:
            options->enable_sandbox = true;
            break;
        case ENABLE_SECCOMP:
            printf("WARNING: Support for seccomp is experimental\n");
            options->enable_seccomp = true;
            break;
        case NETNS_TYPE:
            optarg_netns_type = optarg;
            break;
        case USERNS_PATH:
            optarg_userns_path = optarg;
            if (access(optarg_userns_path, F_OK) == -1) {
                fprintf(stderr, "userns path doesn't exist: %s\n",
                        optarg_userns_path);
                goto error;
            }
            break;
        case 'a':
            optarg_api_socket = optarg;
            break;
        case '6':
            options->enable_ipv6 = true;
            printf("WARNING: Support for IPv6 is experimental\n");
            break;
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            version();
            exit(EXIT_SUCCESS);
            break;
        case OUTBOUND_ADDR:
            printf("WARNING: Support for --outbount-addr is experimental\n");
            optarg_outbound_addr = optarg;
            break;
        case OUTBOUND_ADDR6:
            printf("WARNING: Support for --outbount-addr6 is experimental\n");
            optarg_outbound_addr6 = optarg;
            break;
        case DISABLE_DNS:
            options->disable_dns = true;
            break;
        case MACADDRESS:
            optarg_macaddress = optarg;
            break;
        default:
            goto error;
            break;
        }
    }
    if (optarg_cidr != NULL) {
        options->cidr = strdup(optarg_cidr);
    }
    if (optarg_netns_type != NULL) {
        options->netns_type = strdup(optarg_netns_type);
    }
    if (optarg_userns_path != NULL) {
        options->userns_path = strdup(optarg_userns_path);
    }
    if (optarg_api_socket != NULL) {
        options->api_socket = strdup(optarg_api_socket);
    }
    if (optarg_outbound_addr != NULL) {
        options->outbound_addr = strdup(optarg_outbound_addr);
    }
    if (optarg_outbound_addr6 != NULL) {
        options->outbound_addr6 = strdup(optarg_outbound_addr6);
    }
    if (optarg_macaddress != NULL) {
        if (!options->do_config_network) {
            fprintf(stderr, "--macaddr cannot be specified when --configure or "
                            "-c is not specified\n");
            goto error;
        } else {
            options->macaddress = strdup(optarg_macaddress);
        }
    }
#undef CIDR
#undef DISABLE_HOST_LOOPBACK
#undef NETNS_TYPE
#undef USERNS_PATH
#undef _DEPRECATED_NO_HOST_LOOPBACK
#undef ENABLE_SANDBOX
#undef ENABLE_SECCOMP
#undef OUTBOUND_ADDR
#undef OUTBOUND_ADDR6
#undef DISABLE_DNS
#undef MACADDRESS
    if (argc - optind < 2) {
        goto error;
    }
    if (!options->netns_type ||
        strcmp(options->netns_type, DEFAULT_NETNS_TYPE) == 0) {
        errno = 0;
        options->target_pid = strtol(argv[optind], &strtol_e, 10);
        if (errno || *strtol_e != '\0' || options->target_pid <= 0) {
            fprintf(stderr, "PID must be a positive integer\n");
            goto error;
        }
    } else {
        options->netns_path = strdup(argv[optind]);
        if (access(options->netns_path, F_OK) == -1) {
            perror("existing path expected when --netns-type=path");
            goto error;
        }
    }
    options->tapname = strdup(argv[optind + 1]);
    return;
error:
    usage(argv[0]);
    options_destroy(options);
    exit(EXIT_FAILURE);
}

static int from_regmatch(char *buf, size_t buf_len, regmatch_t match,
                         const char *orig)
{
    size_t len = match.rm_eo - match.rm_so;
    if (len > buf_len - 1) {
        return -1;
    }
    memset(buf, 0, buf_len);
    strncpy(buf, &orig[match.rm_so], len);
    return 0;
}

static int parse_cidr(struct in_addr *network, struct in_addr *netmask,
                      const char *cidr)
{
    int rc = 0;
    regex_t r;
    regmatch_t matches[4];
    size_t nmatch = sizeof(matches) / sizeof(matches[0]);
    const char *cidr_regex = "^(([0-9]{1,3}\\.){3}[0-9]{1,3})/([0-9]{1,2})$";
    char snetwork[16], sprefix[16];
    int prefix;
    rc = regcomp(&r, cidr_regex, REG_EXTENDED);
    if (rc != 0) {
        fprintf(stderr, "internal regex error\n");
        rc = -1;
        goto finish;
    }
    rc = regexec(&r, cidr, nmatch, matches, 0);
    if (rc != 0) {
        fprintf(stderr, "invalid CIDR: %s\n", cidr);
        rc = -1;
        goto finish;
    }
    rc = from_regmatch(snetwork, sizeof(snetwork), matches[1], cidr);
    if (rc < 0) {
        fprintf(stderr, "invalid CIDR: %s\n", cidr);
        goto finish;
    }
    rc = from_regmatch(sprefix, sizeof(sprefix), matches[3], cidr);
    if (rc < 0) {
        fprintf(stderr, "invalid CIDR: %s\n", cidr);
        goto finish;
    }
    if (inet_pton(AF_INET, snetwork, network) != 1) {
        fprintf(stderr, "invalid network address: %s\n", snetwork);
        rc = -1;
        goto finish;
    }
    errno = 0;
    prefix = strtoul(sprefix, NULL, 10);
    if (errno) {
        fprintf(stderr, "invalid prefix length: %s\n", sprefix);
        rc = -1;
        goto finish;
    }
    if (prefix < NETWORK_PREFIX_MIN || prefix > NETWORK_PREFIX_MAX) {
        fprintf(stderr, "prefix length needs to be %d-%d\n", NETWORK_PREFIX_MIN,
                NETWORK_PREFIX_MAX);
        rc = -1;
        goto finish;
    }
    netmask->s_addr = htonl(~((1 << (32 - prefix)) - 1));
    if ((network->s_addr & netmask->s_addr) != network->s_addr) {
        fprintf(stderr, "CIDR needs to be a network address like 10.0.2.0/24, "
                        "not like 10.0.2.100/24\n");
        rc = -1;
        goto finish;
    }
finish:
    regfree(&r);
    return rc;
}

static int slirp4netns_config_from_cidr(struct slirp4netns_config *cfg,
                                        const char *cidr)
{
    int rc;
    rc = parse_cidr(&cfg->vnetwork, &cfg->vnetmask, cidr);
    if (rc < 0) {
        goto finish;
    }
    cfg->vhost.s_addr =
        htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VHOST_OFFSET);
    cfg->vdhcp_start.s_addr =
        htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VDHCPSTART_OFFSET);
    cfg->vnameserver.s_addr =
        htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VNAMESERVER_OFFSET);
    cfg->recommended_vguest.s_addr =
        htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_RECOMMENDED_VGUEST_OFFSET);
finish:
    return rc;
}

static int get_interface_addr(const char *interface, int af, void *addr)
{
    struct ifaddrs *ifaddr, *ifa;
    if (interface == NULL)
        return -1;

    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "getifaddrs failed to obtain interface addresses");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_name == NULL)
            continue;
        if (ifa->ifa_addr->sa_family == af) {
            if (strcmp(ifa->ifa_name, interface) == 0) {
                if (af == AF_INET) {
                    *(struct in_addr *)addr =
                        ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                } else {
                    *(struct in6_addr *)addr =
                        ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
                }
                return 0;
            }
        }
    }
    return -1;
}

/*
 * Convert a MAC string (macaddr) to bytes (data).
 * macaddr must be a null-terminated string in the format of
 * xx:xx:xx:xx:xx:xx.  The data buffer needs to be at least 6 bytes.
 * Typically the data is put into sockaddr sa_data, which is 14 bytes.
 */
static int slirp4netns_macaddr_hexstring_to_data(char *macaddr, char *data)
{
    int temp;
    char *macaddr_ptr;
    char *data_ptr;
    for (macaddr_ptr = macaddr, data_ptr = data;
         macaddr_ptr != NULL && data_ptr < data + 6;
         macaddr_ptr = strchr(macaddr_ptr, ':'), data_ptr++) {
        if (macaddr_ptr != macaddr) {
            macaddr_ptr++; // advance over the :
        }
        if (sscanf(macaddr_ptr, "%x", &temp) != 1 || temp < 0 || temp > 255) {
            fprintf(stderr, "\"%s\" is an invalid MAC address.\n", macaddr);
            return -1;
        }
        *data_ptr = temp;
    }
    if (macaddr_ptr != NULL) {
        fprintf(stderr, "\"%s\" is an invalid MAC address.  Is it too long?\n",
                macaddr);
        return -1;
    }
    return (int)(data_ptr - data);
}

static int slirp4netns_config_from_options(struct slirp4netns_config *cfg,
                                           struct options *opt)
{
    int rc = 0;
    cfg->mtu = opt->mtu;
    rc = slirp4netns_config_from_cidr(cfg, opt->cidr == NULL ? DEFAULT_CIDR :
                                                               opt->cidr);
    if (rc < 0) {
        goto finish;
    }
    cfg->enable_ipv6 = opt->enable_ipv6;
    cfg->disable_host_loopback = opt->disable_host_loopback;
    cfg->enable_sandbox = opt->enable_sandbox;
    cfg->enable_seccomp = opt->enable_seccomp;

#if SLIRP_CONFIG_VERSION_MAX >= 2
    cfg->enable_outbound_addr = false;
    cfg->enable_outbound_addr6 = false;
#endif

    if (opt->outbound_addr != NULL) {
#if SLIRP_CONFIG_VERSION_MAX >= 2
        cfg->outbound_addr.sin_family = AF_INET;
        cfg->outbound_addr.sin_port = 0; // Any local port will do
        if (inet_pton(AF_INET, opt->outbound_addr,
                      &cfg->outbound_addr.sin_addr) == 1) {
            cfg->enable_outbound_addr = true;
        } else {
            if (get_interface_addr(opt->outbound_addr, AF_INET,
                                   &cfg->outbound_addr.sin_addr) != 0) {
                fprintf(stderr, "outbound-addr has to be valid ipv4 address or "
                                "interface name.");
                rc = -1;
                goto finish;
            }
            cfg->enable_outbound_addr = true;
        }
#else
        fprintf(stderr, "slirp4netns has to be compiled against libslrip 4.2.0 "
                        "or newer for --outbound-addr support.");
        rc = -1;
        goto finish;
#endif
    }
    if (opt->outbound_addr6 != NULL) {
#if SLIRP_CONFIG_VERSION_MAX >= 2
        cfg->outbound_addr6.sin6_family = AF_INET6;
        cfg->outbound_addr6.sin6_port = 0; // Any local port will do
        if (inet_pton(AF_INET6, opt->outbound_addr6,
                      &cfg->outbound_addr6.sin6_addr) == 1) {
            cfg->enable_outbound_addr6 = true;
        } else {
            if (get_interface_addr(opt->outbound_addr, AF_INET6,
                                   &cfg->outbound_addr6.sin6_addr) != 0) {
                fprintf(stderr, "outbound-addr has to be valid ipv4 address or "
                                "iterface name.");
                rc = -1;
                goto finish;
            }
            cfg->enable_outbound_addr6 = true;
        }
#else
        fprintf(stderr, "slirp4netns has to be compiled against libslirp 4.2.0 "
                        "or newer for --outbound-addr6 support.");
        rc = -1;
        goto finish;
#endif
    }

#if SLIRP_CONFIG_VERSION_MAX >= 3
    cfg->disable_dns = opt->disable_dns;
#else
    if (opt->disable_dns) {
        fprintf(stderr, "slirp4netns has to be compiled against libslirp 4.3.0 "
                        "or newer for --disable-dns support.");
        rc = -1;
        goto finish;
    }
#endif

    cfg->vmacaddress_len = 0;
    memset(&cfg->vmacaddress, 0, sizeof(cfg->vmacaddress));
    if (opt->macaddress != NULL) {
        cfg->vmacaddress.sa_family = AF_LOCAL;
        int macaddr_len;
        if ((macaddr_len = slirp4netns_macaddr_hexstring_to_data(
                 opt->macaddress, cfg->vmacaddress.sa_data)) < 0) {
            fprintf(stderr, "macaddress has to be a valid MAC address (hex "
                            "string, 6 bytes, each byte separated by a ':').");
            rc = -1;
            goto finish;
        }
        cfg->vmacaddress_len = macaddr_len;
    }
finish:
    return rc;
}

int main(int argc, char *const argv[])
{
    int sv[2];
    pid_t child_pid;
    struct options options;
    struct slirp4netns_config slirp4netns_config;
    int exit_status = 0;

    parse_args(argc, argv, &options);
    if (slirp4netns_config_from_options(&slirp4netns_config, &options) < 0) {
        exit_status = EXIT_FAILURE;
        goto finish;
    }
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit_status = EXIT_FAILURE;
        goto finish;
    }
    if ((child_pid = fork()) < 0) {
        perror("fork");
        exit_status = EXIT_FAILURE;
        goto finish;
    }
    if (child_pid == 0) {
        if (child(sv[1], options.target_pid, options.do_config_network,
                  options.tapname, options.netns_path, options.userns_path,
                  &slirp4netns_config) < 0) {
            exit_status = EXIT_FAILURE;
            goto finish;
        }
    } else {
        int ret, child_wstatus, child_status;
        do
            ret = waitpid(child_pid, &child_wstatus, 0);
        while (ret < 0 && errno == EINTR);
        if (ret < 0) {
            perror("waitpid");
            exit_status = EXIT_FAILURE;
            goto finish;
        }
        if (!WIFEXITED(child_wstatus)) {
            fprintf(stderr, "child failed(wstatus=%d, !WIFEXITED)\n",
                    child_wstatus);
            exit_status = EXIT_FAILURE;
            goto finish;
        }
        child_status = WEXITSTATUS(child_wstatus);
        if (child_status != 0) {
            fprintf(stderr, "child failed(%d)\n", child_status);
            exit_status = child_status;
            goto finish;
        }
        if (parent(sv[0], options.ready_fd, options.exit_fd, options.api_socket,
                   &slirp4netns_config, options.target_pid) < 0) {
            fprintf(stderr, "parent failed\n");
            exit_status = EXIT_FAILURE;
            goto finish;
        }
    }
finish:
    options_destroy(&options);
    exit(exit_status);
    return 0;
}
