#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>

#include "qemu/slirp/slirp.h"
#include "libslirp.h"
#include "api.h"
#include "slirp4netns.h"

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

Slirp *create_slirp(void *opaque, struct slirp_config *cfg)
{
	Slirp *slirp = NULL;
	struct in6_addr vhost6, vprefix_addr6, vnameserver6;
	int vprefix_len = 64;
	inet_pton(AF_INET6, "fd00::2", &vhost6);
	inet_pton(AF_INET6, "fd00::", &vprefix_addr6);
	inet_pton(AF_INET6, "fd00::3", &vnameserver6);
	slirp = slirp_init(0 /* restricted */ , 1 /* is_enabled */ ,
			   cfg->vnetwork, cfg->vnetmask, cfg->vhost, (int)(cfg->enable_ipv6), vprefix_addr6,
			   vprefix_len, vhost6, NULL /* vhostname */ , NULL /* bootfile */ , cfg->vdhcp_start,
			   cfg->vnameserver, vnameserver6, NULL /* vdnssearch */ , NULL /* vdomainname */ ,
			   cfg->mtu /* if_mtu */ , cfg->mtu /* if_mru */ ,
			   cfg->disable_host_loopback, opaque);
	if (slirp == NULL) {
		fprintf(stderr, "slirp_init failed\n");
	}
	return slirp;
}

#define ETH_BUF_SIZE (65536)

int do_slirp(int tapfd, int exitfd, const char *api_socket, struct slirp_config *cfg)
{
	int ret = -1;
	Slirp *slirp = NULL;
	uint8_t *buf = NULL;
	struct libslirp_data opaque = {.tapfd = tapfd };
	int apifd = -1;
	struct api_ctx *apictx = NULL;
	GArray pollfds = { 0 };
	int pollfds_exitfd_idx = -1;
	int pollfds_apifd_idx = -1;
	size_t n_fds = 1;
	struct pollfd tap_pollfd = { tapfd, POLLIN | POLLHUP, 0 };
	struct pollfd exit_pollfd = { exitfd, POLLHUP, 0 };
	struct pollfd api_pollfd = { -1, POLLIN | POLLHUP, 0 };

	slirp = create_slirp((void *)&opaque, cfg);
	if (slirp == NULL) {
		fprintf(stderr, "create_slirp failed\n");
		goto err;
	}
	buf = malloc(ETH_BUF_SIZE);
	if (buf == NULL) {
		goto err;
	}
	g_array_append_val(&pollfds, tap_pollfd);
	if (exitfd >= 0) {
		n_fds++;
		g_array_append_val(&pollfds, exit_pollfd);
		pollfds_exitfd_idx = n_fds - 1;
	}
	if (api_socket != NULL) {
		if ((apifd = api_bindlisten(api_socket)) < 0) {
			goto err;
		}
		if ((apictx = api_ctx_alloc(cfg)) == NULL) {
			fprintf(stderr, "api_ctx_alloc failed\n");
			goto err;
		}
		api_pollfd.fd = apifd;
		n_fds++;
		g_array_append_val(&pollfds, api_pollfd);
		pollfds_apifd_idx = n_fds - 1;
	}
	signal(SIGPIPE, SIG_IGN);
	while (1) {
		int pollout;
		uint32_t timeout = -1;
		pollfds.len = n_fds;
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
			pollout = -1;
		}

		/* The exitfd is closed.  */
		if (pollfds_exitfd_idx >= 0 && pollfds.pfd[pollfds_exitfd_idx].revents) {
			fprintf(stderr, "exitfd event\n");
			goto success;
		}

		if (pollfds_apifd_idx >= 0 && pollfds.pfd[pollfds_apifd_idx].revents) {
			int rc;
			fprintf(stderr, "apifd event\n");
			if ((rc = api_handler(slirp, apifd, apictx)) < 0) {
				fprintf(stderr, "api_handler: rc=%d\n", rc);
			}
		}

		slirp_pollfds_poll(&pollfds, (pollout <= 0));
		check_ra_timeout();
	}
 success:
	ret = 0;
 err:
	fprintf(stderr, "do_slirp is exiting\n");
	if (buf != NULL) {
		free(buf);
	}
	if (apictx != NULL) {
		api_ctx_free(apictx);
		unlink(api_socket);
	}
	if (pollfds.pfd != NULL) {
		free(pollfds.pfd);
		pollfds.len = pollfds.maxlen = 0;
	}
	return ret;
}
