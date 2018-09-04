#ifndef SLIRP_H
# define SLIRP_H

int do_slirp(int tapfd, int exitfd, unsigned int mtu, bool enable_ipv6);

#endif
