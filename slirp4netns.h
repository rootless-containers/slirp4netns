/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SLIRP4NETNS_H
# define SLIRP4NETNS_H
#include <arpa/inet.h>

struct slirp4netns_config {
	unsigned int mtu;
	struct in_addr vnetwork; // 10.0.2.0
	struct in_addr vnetmask; // 255.255.255.0
	struct in_addr vhost; // 10.0.2.2
	struct in_addr vdhcp_start; // 10.0.2.15
	struct in_addr vnameserver; // 10.0.2.3
	struct in_addr recommended_vguest; // 10.0.2.100 (slirp itself is unaware of vguest)
	bool enable_ipv6;
	bool disable_host_loopback;
	bool create_sandbox;
};
int do_slirp(int tapfd, int readyfd, int exitfd, const char *api_socket, struct slirp4netns_config *cfg);

#endif
