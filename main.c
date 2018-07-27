#define _GNU_SOURCE
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
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <stdbool.h>
#include "slirp4netns.h"

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

static int configure_network(const char *tapname)
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

	sai->sin_family = AF_INET;
	sai->sin_port = 0;
	inet_pton(AF_INET, "10.0.2.100", &sai->sin_addr);

	if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
		perror("cannot set device address");
		return -1;
	}

	inet_pton(AF_INET, "255.255.255.0", &sai->sin_addr);
	if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
		perror("cannot set device netmask");
		return -1;
	}

	memset(&route, 0, sizeof(route));
	sai = (struct sockaddr_in *)&route.rt_gateway;
	sai->sin_family = AF_INET;
	inet_pton(AF_INET, "10.0.2.2", &sai->sin_addr);
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

static int child(int sock, pid_t target_pid, bool do_config_network, const char *tapname, int ready_fd)
{
	int rc, tapfd;
	if ((rc = nsenter(target_pid)) < 0) {
		return rc;
	}
	if ((tapfd = open_tap(tapname)) < 0) {
		return tapfd;
	}
	if (do_config_network && configure_network(tapname) < 0) {
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

static int parent(int sock, int exit_fd)
{
	int rc, tapfd;
	if ((tapfd = recvfd(sock)) < 0) {
		return tapfd;
	}
	fprintf(stderr, "received tapfd=%d\n", tapfd);
	close(sock);
	if ((rc = do_slirp(tapfd, exit_fd)) < 0) {
		fprintf(stderr, "do_slirp failed\n");
		close(tapfd);
		return rc;
	}
	/* NOT REACHED */
	return 0;
}

static void usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-c] [-e FD] [-r FD] PID TAPNAME\n", argv0);
}

// caller needs to free tapname
static void parse_args(int argc, char *const argv[], pid_t *ptarget_pid, char **tapname, bool * do_config_network,
		       int *exit_fd, int *ready_fd)
{
	int opt;
	int target_pid;

	while ((opt = getopt(argc, argv, "ce:r:")) != -1) {
		switch (opt) {
		case 'c':
			*do_config_network = true;
			break;
		case 'e':
			errno = 0;
			*exit_fd = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'r':
			errno = 0;
			*ready_fd = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (*ready_fd >= 0 && !*do_config_network) {
		fprintf(stderr, "the option -r FD requires -c\n");
		exit(EXIT_FAILURE);
	}
	if (argc - optind < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	errno = 0;
	target_pid = strtol(argv[optind], NULL, 10);
	if (errno != 0) {
		perror("strtol");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	*ptarget_pid = (pid_t)target_pid;
	*tapname = strdup(argv[optind + 1]);
}

int main(int argc, char *const argv[])
{
	int sv[2];
	pid_t target_pid, child_pid;
	char *tapname;
	int exit_fd = -1;
	int ready_fd = -1;
	bool do_config_network = false;

	parse_args(argc, argv, &target_pid, &tapname, &do_config_network, &exit_fd, &ready_fd);
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) < 0) {
		perror("socketpair");
		exit(EXIT_FAILURE);
	}
	if ((child_pid = fork()) < 0) {
		perror("fork");
		free(tapname);
		tapname = NULL;
		exit(EXIT_FAILURE);
	}
	if (child_pid == 0) {
		if (child(sv[1], target_pid, do_config_network, tapname, ready_fd) < 0) {
			free(tapname);
			tapname = NULL;
			exit(EXIT_FAILURE);
		}
		free(tapname);
		tapname = NULL;
	} else {
		int child_wstatus, child_status;
		free(tapname);
		tapname = NULL;
		waitpid(child_pid, &child_wstatus, 0);
		if (!WIFEXITED(child_wstatus)) {
			fprintf(stderr, "child failed\n");
			exit(EXIT_FAILURE);
		}
		child_status = WEXITSTATUS(child_wstatus);
		if (child_status != 0) {
			fprintf(stderr, "child failed(%d)\n", child_status);
			exit(child_status);
		}
		if (parent(sv[0], exit_fd) < 0) {
			fprintf(stderr, "parent failed\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}
