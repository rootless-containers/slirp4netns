/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SLIRP4NETNS_H
#define SLIRP4NETNS_H
#include <arpa/inet.h>

struct slirp4netns_config {
    unsigned int mtu;
    struct in_addr vnetwork; // 10.0.2.0
    struct in_addr vnetmask; // 255.255.255.0
    struct in_addr vhost; // 10.0.2.2
    struct in_addr vdhcp_start; // 10.0.2.15
    struct in_addr vnameserver; // 10.0.2.3
    struct in_addr
        recommended_vguest; // 10.0.2.100 (slirp itself is unaware of vguest)
    struct in6_addr vnetwork6; // fd00:RANDOM:0
    struct in6_addr vnetmask6; // /64
    struct in6_addr vhost6; // fd00:RANDOM:2
    struct in6_addr vdhcp_start6; // fd00:RANDOM:15
    struct in6_addr vnameserver6; // fd00:RANDOM:3
    struct in6_addr
        recommended_vguest6; // fdd00::RANDOM:100 (slirp itself is unaware of vguest)
    bool enable_ipv6;
    bool disable_host_loopback;
    bool enable_sandbox;
    bool enable_seccomp;
#if SLIRP_CONFIG_VERSION_MAX >= 2
    bool enable_outbound_addr;
    struct sockaddr_in outbound_addr;
    bool enable_outbound_addr6;
    struct sockaddr_in6 outbound_addr6;
#endif
#if SLIRP_CONFIG_VERSION_MAX >= 3
    bool disable_dns;
#endif
    struct sockaddr vmacaddress; // MAC address of interface
    int vmacaddress_len; // MAC address byte length
};
int do_slirp(int tapfd, int readyfd, int exitfd, const char *api_socket,
             struct slirp4netns_config *cfg);

#endif
