#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "slirp.h"
#include "libslirp.h"

struct libslirp_data {
	int tapfd;
};

void slirp_output(void *opaque, const uint8_t * pkt, int pkt_len)
{
	struct libslirp_data *data = (struct libslirp_data *)opaque;
	int rc;
	if ((rc = write(data->tapfd, pkt, pkt_len)) < 0) {
		perror("slirp_output: write");
	}
	assert(rc == pkt_len);
}

Slirp *create_slirp(void *opaque)
{
	Slirp *slirp = NULL;
	struct in_addr vnetwork, vnetmask, vhost, vdhcp_start, vnameserver;
	struct in6_addr vhost6, vprefix_addr6, vnameserver6;	/* ignored */
	inet_pton(AF_INET, "10.0.2.0", &vnetwork);
	inet_pton(AF_INET, "255.255.255.0", &vnetmask);
	inet_pton(AF_INET, "10.0.2.2", &vhost);
	inet_pton(AF_INET, "10.0.2.3", &vnameserver);
	inet_pton(AF_INET, "10.0.2.15", &vdhcp_start);
	slirp = slirp_init(0 /* restricted */ , 1 /* is_enabled */ ,
			   vnetwork, vnetmask, vhost, 0 /* ip6_enabled */ , vprefix_addr6, 0 /* vprefix_len */ , vhost6,
			   NULL /* vhostname */ , NULL /* bootfile */ , vdhcp_start,
			   vnameserver, vnameserver6, NULL /* vdnssearch */ , NULL /* vdomainname */ ,
			   opaque);
	if (slirp == NULL) {
		fprintf(stderr, "slirp_init failed\n");
	}
	return slirp;
}

#define ETH_BUF_SIZE (65536)

int do_slirp(int tapfd)
{
	Slirp *slirp = NULL;
	uint8_t *buf = NULL;
	struct libslirp_data opaque = {.tapfd = tapfd };
	GArray pollfds;
	struct pollfd tap_pollfd = { tapfd, POLLIN | POLLHUP, 0 };
	slirp = create_slirp((void *)&opaque);
	if (slirp == NULL) {
		fprintf(stderr, "create_slirp failed\n");
		goto err;
	}
	buf = malloc(ETH_BUF_SIZE);
	if (buf == NULL) {
		goto err;
	}
	g_array_append_val(&pollfds, tap_pollfd);
	while (1) {
		int pollout;
		uint32_t timeout = -1;
		pollfds.len = 1;
		slirp_pollfds_fill(&pollfds, &timeout);
		update_ra_timeout(&timeout);
		do
			pollout = poll(pollfds.pfd, pollfds.len, timeout);
		while (pollout < 0 && errno == EINTR);
		if (pollout < 0) {
			goto err;
		}
		if (pollfds.pfd[0].revents) {
			ssize_t rc = read(tapfd, buf, ETH_BUF_SIZE);
			if (rc < 0) {
				perror("do_slirp: read");
				goto after_slirp_input;
			}
			slirp_input(slirp, buf, (int)rc);
 after_slirp_input:
			pollout--;
		}
		slirp_pollfds_poll(&pollfds, (pollout <= 0));
		check_ra_timeout();
	}
 err:
	fprintf(stderr, "do_slirp is exiting\n");
	if (buf != NULL) {
		free(buf);
	}
	return -1;
}
