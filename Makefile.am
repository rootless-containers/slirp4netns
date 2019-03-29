bin_PROGRAMS = slirp4netns

AM_CFLAGS = @GLIB_CFLAGS@

noinst_LIBRARIES = libqemu_slirp.a libparson.a

AM_TESTS_ENVIRONMENT = PATH="$(abs_top_builddir):$(PATH)"
TESTS = tests/test-slirp4netns.sh tests/test-slirp4netns-configure.sh tests/test-slirp4netns-exit-fd.sh tests/test-slirp4netns-ready-fd.sh tests/test-slirp4netns-api-socket.sh tests/test-slirp4netns-disable-host-loopback.sh tests/test-slirp4netns-cidr.sh

EXTRA_DIST = \
	slirp4netns.1.md \
	slirp4netns.1 \
	$(TESTS) \
	tests/common.sh \
	slirp4netns.h \
	api.h \
	qemu/slirp/src/bootp.h \
	qemu/slirp/src/debug.h \
	qemu/slirp/src/dhcpv6.h \
	qemu/slirp/src/if.h \
	qemu/slirp/src/ip6.h \
	qemu/slirp/src/ip6_icmp.h \
	qemu/slirp/src/ip.h \
	qemu/slirp/src/ip_icmp.h \
	qemu/slirp/src/libslirp.h \
	qemu/slirp/src/main.h \
	qemu/slirp/src/mbuf.h \
	qemu/slirp/src/misc.h \
	qemu/slirp/src/ncsi-pkt.h \
	qemu/slirp/src/qtailq.h \
	qemu/slirp/src/sbuf.h \
	qemu/slirp/src/slirp.h \
	qemu/slirp/src/socket.h \
	qemu/slirp/src/stream.h \
	qemu/slirp/src/tftp.h \
	qemu/slirp/src/tcp.h \
	qemu/slirp/src/tcpip.h \
	qemu/slirp/src/tcp_timer.h \
	qemu/slirp/src/tcp_var.h \
	qemu/slirp/src/udp.h \
	qemu/slirp/src/util.h \
	qemu/slirp/src/vmstate.h \
	parson/parson.h

libqemu_slirp_a_SOURCES = \
			qemu/slirp/src/arp_table.c \
			qemu/slirp/src/bootp.c \
			qemu/slirp/src/cksum.c \
			qemu/slirp/src/dhcpv6.c \
			qemu/slirp/src/dnssearch.c \
			qemu/slirp/src/if.c \
			qemu/slirp/src/ip6_icmp.c \
			qemu/slirp/src/ip6_input.c \
			qemu/slirp/src/ip6_output.c \
			qemu/slirp/src/ip_icmp.c \
			qemu/slirp/src/ip_input.c \
			qemu/slirp/src/ip_output.c \
			qemu/slirp/src/mbuf.c \
			qemu/slirp/src/misc.c \
			qemu/slirp/src/ncsi.c \
			qemu/slirp/src/ndp_table.c \
			qemu/slirp/src/sbuf.c \
			qemu/slirp/src/slirp.c \
			qemu/slirp/src/socket.c \
			qemu/slirp/src/state.c \
			qemu/slirp/src/stream.c \
			qemu/slirp/src/tcp_input.c \
			qemu/slirp/src/tcp_output.c \
			qemu/slirp/src/tcp_subr.c \
			qemu/slirp/src/tcp_timer.c \
			qemu/slirp/src/tftp.c \
			qemu/slirp/src/udp6.c \
			qemu/slirp/src/udp.c \
			qemu/slirp/src/util.c \
			qemu/slirp/src/vmstate.c

# define specific commit if git available or it was replaced during git-archive creation
COMMIT := $(shell V=$Format:%H$ ; \
	expr match "$$V" ormat: >/dev/null \
		&& (cd "$$abs_srcdir" && [ -d .git ] && git describe --always --abbrev=0 --dirty --exclude=\* || echo unknown) \
		|| echo "$$V" )
DEFINE_COMMIT = -DCOMMIT="\"$(COMMIT)\""

slirp4netns_CFLAGS = $(AM_CFLAGS) $(DEFINE_COMMIT)
libqemu_slirp_a_CFLAGS = $(AM_CFLAGS) -I$(abs_top_builddir)/qemu/slirp/src
libparson_a_CFLAGS = $(AM_CFLAGS) -I$(abs_top_builddir)/parson
libparson_a_SOURCES = parson/parson.c

slirp4netns_SOURCES = main.c slirp4netns.c api.c
slirp4netns_LDADD = libqemu_slirp.a libparson.a @GLIB_LIBS@ -lpthread
man1_MANS = slirp4netns.1

generate-man:
	go-md2man -in slirp4netns.1.md -out slirp4netns.1

CLANGTIDY = clang-tidy -warnings-as-errors='*'

lint:
	$(CLANGTIDY) $(slirp4netns_SOURCES) -- $(AM_CFLAGS)

lint-full:
	$(CLANGTIDY) $(slirp4netns_SOURCES) $(libqemu_slirp_a_SOURCES) $(libparson_a_SOURCES) -- $(AM_CFLAGS)

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
# TODO: make sure ./qemu is synced with ./qemu_patches/sync.sh
# (hard to verify during `make`, because sync.sh removes ./qemu/slirp/src/.deps)
	$(MAKE) lint
	$(MAKE) -j $(shell nproc) distcheck || ( find . -name test-suite.log | xargs cat; exit 1 )
	PATH=$(shell pwd):$$PATH $(MAKE) benchmark MTU=1500
	PATH=$(shell pwd):$$PATH $(MAKE) benchmark MTU=65520

.PHONY: generate-man lint lint-full indent benchmark ci
