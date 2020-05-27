/* SPDX-License-Identifier: LGPL-2.1+ */
#ifndef SLIRP4NETNS_SECCOMPARCH_H
#define SLIRP4NETNS_SECCOMPARCH_H

#include <stdint.h>
#include <seccomp.h>

/*
 * seccomp_extra_archs derived from systemd seccomp_local_archs
 * but does NOT contain native archs:
 * https://github.com/systemd/systemd/blob/v245/src/shared/seccomp-util.c#L25-L95
 * .
 */
const uint32_t seccomp_extra_archs[] = {
#if defined(__x86_64__) && \
    defined(__ILP32__) /* X32 ( https://en.wikipedia.org/wiki/X32_ABI ) */
    SCMP_ARCH_X86, SCMP_ARCH_X86_64,
#elif defined(__x86_64__) && !defined(__ILP32__) /* X86_64 */
    SCMP_ARCH_X86, SCMP_ARCH_X32,
#elif defined(__i386__) /* X86 */
/* NONE */
#elif defined(__aarch64__) /* AARCH64 */
    SCMP_ARCH_ARM,
#elif defined(__arm__) /* ARM */
/* NONE */
#elif defined(__mips__) && __BYTE_ORDER == __BIG_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_ABI32 /* MIPS */
    SCMP_ARCH_MIPSEL,
#elif defined(__mips__) && __BYTE_ORDER == __LITTLE_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_ABI32 /* MIPSEL */
    SCMP_ARCH_MIPS,
#elif defined(__mips__) && __BYTE_ORDER == __BIG_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_ABI64 /* MIPS64 */
    SCMP_ARCH_MIPSEL,    SCMP_ARCH_MIPS,     SCMP_ARCH_MIPSEL64N32,
    SCMP_ARCH_MIPS64N32, SCMP_ARCH_MIPSEL64,
#elif defined(__mips__) && __BYTE_ORDER == __LITTLE_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_ABI64 /* MIPSEL64 */
    SCMP_ARCH_MIPS,        SCMP_ARCH_MIPSEL, SCMP_ARCH_MIPS64N32,
    SCMP_ARCH_MIPSEL64N32, SCMP_ARCH_MIPS64,
#elif defined(__mips__) && __BYTE_ORDER == __BIG_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_NABI32 /* MIPS64N32 */
    SCMP_ARCH_MIPSEL, SCMP_ARCH_MIPS,        SCMP_ARCH_MIPSEL64,
    SCMP_ARCH_MIPS64, SCMP_ARCH_MIPSEL64N32,
#elif defined(__mips__) && __BYTE_ORDER == __LITTLE_ENDIAN && \
    _MIPS_SIM == _MIPS_SIM_NABI32 /* MIPSEL64N32 */
    SCMP_ARCH_MIPS,     SCMP_ARCH_MIPSEL,    SCMP_ARCH_MIPS64,
    SCMP_ARCH_MIPSEL64, SCMP_ARCH_MIPS64N32,
#elif defined(__powerpc64__) && __BYTE_ORDER == __BIG_ENDIAN /* PPC64 */
    SCMP_ARCH_PPC, SCMP_ARCH_PPC64LE,
#elif defined(__powerpc64__) && __BYTE_ORDER == __LITTLE_ENDIAN /* PPC64LE */
    SCMP_ARCH_PPC, SCMP_ARCH_PPC64,
#elif defined(__powerpc__) /* PPC */
/* NONE */
#elif defined(__s390x__) /* S390X */
    SCMP_ARCH_S390,
#elif defined(__s390__) /* S390 */
/* NONE */
#endif
    (uint32_t)-1
};

/* seccomp_extra_archs_items can be 0 */
const int seccomp_extra_archs_items =
    sizeof(seccomp_extra_archs) / sizeof(seccomp_extra_archs[0]) - 1;

#endif
