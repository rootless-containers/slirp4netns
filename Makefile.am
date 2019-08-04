bin_PROGRAMS = slirp4netns

AM_CFLAGS = @GLIB_CFLAGS@

noinst_LIBRARIES = libslirp.a libparson.a

AM_TESTS_ENVIRONMENT = PATH="$(abs_top_builddir):$(PATH)"
TESTS = tests/test-slirp4netns.sh tests/test-slirp4netns-configure.sh tests/test-slirp4netns-exit-fd.sh tests/test-slirp4netns-ready-fd.sh tests/test-slirp4netns-api-socket.sh tests/test-slirp4netns-disable-host-loopback.sh tests/test-slirp4netns-cidr.sh

EXTRA_DIST = \
	slirp4netns.1.md \
	slirp4netns.1 \
	$(TESTS) \
	tests/common.sh \
	slirp4netns.h \
	api.h \
	vendor/libslirp/COPYRIGHT \
	vendor/libslirp/README.md \
	vendor/libslirp/src/bootp.h \
	vendor/libslirp/src/debug.h \
	vendor/libslirp/src/dhcpv6.h \
	vendor/libslirp/src/if.h \
	vendor/libslirp/src/ip6.h \
	vendor/libslirp/src/ip6_icmp.h \
	vendor/libslirp/src/ip.h \
	vendor/libslirp/src/ip_icmp.h \
	vendor/libslirp/src/libslirp.h \
	vendor/libslirp/src/libslirp-version.h \
	vendor/libslirp/src/main.h \
	vendor/libslirp/src/mbuf.h \
	vendor/libslirp/src/misc.h \
	vendor/libslirp/src/ncsi-pkt.h \
	vendor/libslirp/src/sbuf.h \
	vendor/libslirp/src/slirp.h \
	vendor/libslirp/src/socket.h \
	vendor/libslirp/src/stream.h \
	vendor/libslirp/src/tftp.h \
	vendor/libslirp/src/tcp.h \
	vendor/libslirp/src/tcpip.h \
	vendor/libslirp/src/tcp_timer.h \
	vendor/libslirp/src/tcp_var.h \
	vendor/libslirp/src/udp.h \
	vendor/libslirp/src/util.h \
	vendor/libslirp/src/vmstate.h \
	vendor/parson/LICENSE \
	vendor/parson/README.md \
	vendor/parson/parson.h

libslirp_a_SOURCES = \
			vendor/libslirp/src/arp_table.c \
			vendor/libslirp/src/bootp.c \
			vendor/libslirp/src/cksum.c \
			vendor/libslirp/src/dhcpv6.c \
			vendor/libslirp/src/dnssearch.c \
			vendor/libslirp/src/if.c \
			vendor/libslirp/src/ip6_icmp.c \
			vendor/libslirp/src/ip6_input.c \
			vendor/libslirp/src/ip6_output.c \
			vendor/libslirp/src/ip_icmp.c \
			vendor/libslirp/src/ip_input.c \
			vendor/libslirp/src/ip_output.c \
			vendor/libslirp/src/mbuf.c \
			vendor/libslirp/src/misc.c \
			vendor/libslirp/src/ncsi.c \
			vendor/libslirp/src/ndp_table.c \
			vendor/libslirp/src/sbuf.c \
			vendor/libslirp/src/slirp.c \
			vendor/libslirp/src/socket.c \
			vendor/libslirp/src/state.c \
			vendor/libslirp/src/stream.c \
			vendor/libslirp/src/tcp_input.c \
			vendor/libslirp/src/tcp_output.c \
			vendor/libslirp/src/tcp_subr.c \
			vendor/libslirp/src/tcp_timer.c \
			vendor/libslirp/src/tftp.c \
			vendor/libslirp/src/udp6.c \
			vendor/libslirp/src/udp.c \
			vendor/libslirp/src/util.c \
			vendor/libslirp/src/version.c \
			vendor/libslirp/src/vmstate.c

# define specific commit if git available or it was replaced during git-archive creation
COMMIT := $(shell V=$Format:%H$ ; \
	expr match "$$V" ormat: >/dev/null \
		&& (cd "$$abs_srcdir" && [ -d .git ] && git describe --always --abbrev=0 --dirty --exclude=\* || echo unknown) \
		|| echo "$$V" )
DEFINE_COMMIT = -DCOMMIT="\"$(COMMIT)\""

slirp4netns_CFLAGS = $(AM_CFLAGS) $(DEFINE_COMMIT)
libslirp_a_CFLAGS = $(AM_CFLAGS) -I$(abs_top_builddir)/vendor/libslirp/src
libparson_a_CFLAGS = $(AM_CFLAGS) -I$(abs_top_builddir)/vendor/parson
libparson_a_SOURCES = vendor/parson/parson.c

slirp4netns_SOURCES = main.c slirp4netns.c api.c
slirp4netns_LDADD = libslirp.a libparson.a @GLIB_LIBS@ -lpthread
man1_MANS = slirp4netns.1

generate-man:
	go-md2man -in slirp4netns.1.md -out slirp4netns.1

CLANGTIDY = clang-tidy -warnings-as-errors='*'

lint:
	$(CLANGTIDY) $(slirp4netns_SOURCES) -- $(AM_CFLAGS)

lint-full:
	$(CLANGTIDY) $(slirp4netns_SOURCES) $(libslirp_a_SOURCES) $(libparson_a_SOURCES) -- $(AM_CFLAGS)

indent:
# indent(1): "You must use the ‘-T’ option to tell indent the name of all the typenames in your program that are defined by typedef."
	indent -linux -l120 \
		-T ssize_t -T pid_t \
		-T GArray -T GList -T GSList -T GPollFD -T gpointer -T gconstpointer \
		-T Slirp -T SlirpCb -T SlirpConfig -T SlirpTimerCb \
		-T JSON_Object -T JSON_Value \
		-T regex_t -T regmatch_t \
		$(slirp4netns_SOURCES)
	$(RM) *.c~


benchmark:
	benchmarks/benchmark-iperf3.sh
	benchmarks/benchmark-iperf3-reverse.sh

ci:
	$(MAKE) indent
	test -z "$(git diff)"
# TODO: make sure ./vendor is synced with ./vendor.sh
# (hard to verify during `make`, because sync.sh removes ./vendor/libslirp/src/.deps)
	$(MAKE) lint
	$(MAKE) -j $(shell nproc) distcheck || ( find . -name test-suite.log | xargs cat; exit 1 )
	PATH=$(shell pwd):$$PATH $(MAKE) benchmark MTU=1500
	PATH=$(shell pwd):$$PATH $(MAKE) benchmark MTU=65520

.PHONY: generate-man lint lint-full indent benchmark ci
