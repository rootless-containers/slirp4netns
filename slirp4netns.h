#ifndef SLIRP4NETNS_H
# define SLIRP4NETNS_H

struct slirp_config {
	unsigned int mtu;
	bool enable_ipv6;
	bool no_host_loopback;
};
int do_slirp(int tapfd, int exitfd, const char *api_socket, struct slirp_config *cfg);

#endif
