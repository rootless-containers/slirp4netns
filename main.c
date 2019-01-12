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
#include "slirp4netns.h"

#define DEFAULT_MTU (1500)
#define DEFAULT_CIDR ("10.0.2.0/24")
#define DEFAULT_VHOST_OFFSET (2)	// 10.0.2.2
#define DEFAULT_VDHCPSTART_OFFSET (15)	// 10.0.2.15
#define DEFAULT_VNAMESERVER_OFFSET (3)	// 10.0.2.3
#define DEFAULT_RECOMMENDED_VGUEST_OFFSET (100)	// 10.0.2.100
#define NETWORK_PREFIX_MIN (1)
// >=26 is not supported because the recommended guest IP is set to network addr + 100 .
#define NETWORK_PREFIX_MAX (25)

static int nsenter(pid_t target_pid)
{
	char userns[32], netns[32];
	int usernsfd, netnsfd;
	snprintf(userns, sizeof(userns), "/proc/%d/ns/user", target_pid);
	snprintf(netns, sizeof(netns), "/proc/%d/ns/net", target_pid);
	if ((usernsfd = open(userns, O_RDONLY)) < 0) {
		perror(userns);
		return usernsfd;
	}
	if ((netnsfd = open(netns, O_RDONLY)) < 0) {
		perror(netns);
		return netnsfd;
	}
	setns(usernsfd, CLONE_NEWUSER);
	if (setns(netnsfd, CLONE_NEWNET) < 0) {
		perror("setns(CLONE_NEWNET)");
		return -1;
	}
	close(usernsfd);
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

static int configure_network(const char *tapname, struct slirp_config *cfg)
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

static int child(int sock, pid_t target_pid, bool do_config_network, const char *tapname, int ready_fd,
		 struct slirp_config *cfg)
{
	int rc, tapfd;
	if ((rc = nsenter(target_pid)) < 0) {
		return rc;
	}
	if ((tapfd = open_tap(tapname)) < 0) {
		return tapfd;
	}
	if (do_config_network && configure_network(tapname, cfg) < 0) {
		return -1;
	}
	if (ready_fd >= 0) {
		do
			rc = write(ready_fd, "1", 1);
		while (rc < 0 && errno == EINTR);
		close(ready_fd);
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

static int parent(int sock, int exit_fd, const char *api_socket, struct slirp_config *cfg)
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
	if (!cfg->disable_host_loopback) {
		printf
		    ("WARNING: 127.0.0.1:* on the host is accessible as %s (set --disable-host-loopback to prohibit connecting to 127.0.0.1:*)\n",
		     inet_ntoa(cfg->vhost));
	}
	if ((rc = do_slirp(tapfd, exit_fd, api_socket, cfg)) < 0) {
		fprintf(stderr, "do_slirp failed\n");
		close(tapfd);
		return rc;
	}
	/* NOT REACHED */
	return 0;
}

static void usage(const char *argv0)
{
	printf("Usage: %s [OPTION]... PID TAPNAME\n", argv0);
	printf("User-mode networking for unprivileged network namespaces.\n\n");
	printf("-c, --configure          bring up the interface\n");
	printf("-e, --exit-fd=FD         specify the FD for terminating slirp4netns\n");
	printf("-r, --ready-fd=FD        specify the FD to write to when the network is configured\n");
	printf("-m, --mtu=MTU            specify MTU (default=%d, max=65521)\n", DEFAULT_MTU);
	printf("--cidr=CIDR              specify network address CIDR (default=%s)\n", DEFAULT_CIDR);
	printf("--disable-host-loopback  prohibit connecting to 127.0.0.1:* on the host namespace\n");
	printf("-a, --api-socket=PATH    specify API socket path (experimental)\n");
	printf("-6, --enable-ipv6        enable IPv6 (experimental)\n");
	printf("-h, --help               show this help and exit\n");
	printf("-v, --version            show version and exit\n");
}

// version output is runc-compatible and machine-parsable
static void version()
{
	printf("slirp4netns version %s\n", VERSION ? VERSION : PACKAGE_VERSION);
#ifdef COMMIT
	printf("commit: %s\n", COMMIT);
#endif
}

struct options {
	pid_t target_pid;	// argv[1]
	char *tapname;		// argv[2]
	bool do_config_network;	// -c
	int exit_fd;		// -e
	int ready_fd;		// -r
	unsigned int mtu;	// -m
	bool disable_host_loopback;	// --disable-host-loopback
	char *cidr;		// --cidr
	bool enable_ipv6;	// -6
	char *api_socket;	// -a
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
	// options itself is not freed, because it can be on the stack.
}

// * caller does not need to call options_init()
// * caller needs to call options_destroy() after calling this function.
// * this function calls exit() on an error.
static void parse_args(int argc, char *const argv[], struct options *options)
{
	int opt;
#define CIDR -42
#define DISABLE_HOST_LOOPBACK -43
#define _DEPRECATED_NO_HOST_LOOPBACK -10043	// deprecated in favor of disable-host-loopback
	const struct option longopts[] = {
		{"configure", no_argument, NULL, 'c'},
		{"exit-fd", required_argument, NULL, 'e'},
		{"ready-fd", required_argument, NULL, 'r'},
		{"mtu", required_argument, NULL, 'm'},
		{"cidr", required_argument, NULL, CIDR},
		{"disable-host-loopback", no_argument, NULL, DISABLE_HOST_LOOPBACK},
		{"no-host-loopback", no_argument, NULL, _DEPRECATED_NO_HOST_LOOPBACK},
		{"api-socket", required_argument, NULL, 'a'},
		{"enable-ipv6", no_argument, NULL, '6'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0},
	};
	options_init(options);
	while ((opt = getopt_long(argc, argv, "ce:r:m:a:6hv", longopts, NULL)) != -1) {
		switch (opt) {
		case 'c':
			options->do_config_network = true;
			break;
		case 'e':
			errno = 0;
			options->exit_fd = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'r':
			errno = 0;
			options->ready_fd = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'm':
			errno = 0;
			options->mtu = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case CIDR:
			options->cidr = strdup(optarg);
			break;
		case _DEPRECATED_NO_HOST_LOOPBACK:
			// There was no tagged release with support for --no-host-loopback.
			// So no one will be affected by removal of --no-host-loopback.
			printf
			    ("WARNING: --no-host-loopback is deprecated and will be removed in future releases, please use --disable-host-loopback instead.\n");
			/* FALLTHROUGH */
		case DISABLE_HOST_LOOPBACK:
			options->disable_host_loopback = true;
			break;
		case 'a':
			options->api_socket = strdup(optarg);
			printf("WARNING: Support for API socket is experimental\n");
			break;
		case '6':
			options->enable_ipv6 = true;
			printf("WARNING: Support for IPv6 is experimental\n");
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case 'v':
			version();
			exit(EXIT_SUCCESS);
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
#undef CIDR
#undef DISABLE_HOST_LOOPBACK
#undef _DEPRECATED_NO_HOST_LOOPBACK
	if (options->ready_fd >= 0 && !options->do_config_network) {
		fprintf(stderr, "the option -r FD requires -c\n");
		exit(EXIT_FAILURE);
	}
	if (options->mtu == 0 || options->mtu > 65521) {
		fprintf(stderr, "MTU must be a positive integer (< 65522)\n");
		exit(EXIT_FAILURE);
	}
	if (argc - optind < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	errno = 0;
	options->target_pid = strtol(argv[optind], NULL, 10);
	if (errno != 0) {
		perror("strtol");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	options->tapname = strdup(argv[optind + 1]);
}

static int from_regmatch(char *buf, size_t buf_len, regmatch_t match, const char *orig)
{
	size_t len = match.rm_eo - match.rm_so;
	if (len > buf_len - 1) {
		return -1;
	}
	memset(buf, 0, buf_len);
	strncpy(buf, &orig[match.rm_so], len);
	return 0;
}

static int parse_cidr(struct in_addr *network, struct in_addr *netmask, const char *cidr)
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
		fprintf(stderr, "prefix length needs to be %d-%d\n", NETWORK_PREFIX_MIN, NETWORK_PREFIX_MAX);
		rc = -1;
		goto finish;
	}
	netmask->s_addr = htonl(~((1 << (32 - prefix)) - 1));
	if ((network->s_addr & netmask->s_addr) != network->s_addr) {
		fprintf(stderr, "CIDR needs to be a network address like 10.0.2.0/24, not like 10.0.2.100/24\n");
		rc = -1;
		goto finish;
	}
 finish:
	regfree(&r);
	return rc;
}

static int slirp_config_from_cidr(struct slirp_config *cfg, const char *cidr)
{
	int rc;
	rc = parse_cidr(&cfg->vnetwork, &cfg->vnetmask, cidr);
	if (rc < 0) {
		goto finish;
	}
	cfg->vhost.s_addr = htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VHOST_OFFSET);
	cfg->vdhcp_start.s_addr = htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VDHCPSTART_OFFSET);
	cfg->vnameserver.s_addr = htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_VNAMESERVER_OFFSET);
	cfg->recommended_vguest.s_addr = htonl(ntohl(cfg->vnetwork.s_addr) + DEFAULT_RECOMMENDED_VGUEST_OFFSET);
 finish:
	return rc;
}

static int slirp_config_from_options(struct slirp_config *cfg, struct options *opt)
{
	int rc = 0;
	cfg->mtu = opt->mtu;
	rc = slirp_config_from_cidr(cfg, opt->cidr == NULL ? DEFAULT_CIDR : opt->cidr);
	if (rc < 0) {
		goto finish;
	}
	cfg->enable_ipv6 = cfg->enable_ipv6;
	cfg->disable_host_loopback = opt->disable_host_loopback;
 finish:
	return rc;
}

int main(int argc, char *const argv[])
{
	int sv[2];
	pid_t child_pid;
	struct options options;
	struct slirp_config slirp_config;
	int exit_status = 0;

	parse_args(argc, argv, &options);
	if (slirp_config_from_options(&slirp_config, &options) < 0) {
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
		if (child(sv[1], options.target_pid, options.do_config_network, options.tapname, options.ready_fd,
			  &slirp_config) < 0) {
			exit_status = EXIT_FAILURE;
			goto finish;
		}
	} else {
		int child_wstatus, child_status;
		waitpid(child_pid, &child_wstatus, 0);
		if (!WIFEXITED(child_wstatus)) {
			fprintf(stderr, "child failed\n");
			exit_status = EXIT_FAILURE;
			goto finish;
		}
		child_status = WEXITSTATUS(child_wstatus);
		if (child_status != 0) {
			fprintf(stderr, "child failed(%d)\n", child_status);
			exit_status = child_status;
			goto finish;
		}
		if (parent(sv[0], options.exit_fd, options.api_socket, &slirp_config) < 0) {
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
