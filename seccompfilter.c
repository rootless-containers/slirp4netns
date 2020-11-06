/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <seccomp.h>
#include "seccomparch.h"

int enable_seccomp()
{
    int rc = -1, i;
    /* Allow everything by default and block dangerous syscalls explicitly,
     * as it is hard to find the correct set of required syscalls */
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL)
        goto ret;
    for (i = 0; i < seccomp_extra_archs_items; i++) {
        uint32_t arch = seccomp_extra_archs[i];
        rc = seccomp_arch_add(ctx, arch);
        if (rc < 0 && rc != -EEXIST && rc != -EDOM) {
            fprintf(stderr,
                    "seccomp: WARNING: can't add extra arch (i=%d): %s\n", i,
                    strerror(-rc));
        }
    }
    printf("seccomp: The following syscalls will be blocked by seccomp:");
/* Setting BLOCK_ACTION to SCMP_ACT_KILL_PROCESS results in issues reported at:
 *  - https://github.com/rootless-containers/slirp4netns/issues/221
 *  - https://github.com/containers/podman/issues/6967 */
#define BLOCK_ACTION SCMP_ACT_KILL
#define BLOCK(x)                                                  \
    {                                                             \
        rc = seccomp_rule_add(ctx, BLOCK_ACTION, SCMP_SYS(x), 0); \
        if (rc < 0)                                               \
            goto ret;                                             \
        printf(" %s", #x);                                        \
    }
    BLOCK(execve);
#ifdef __NR_execveat
    BLOCK(execveat);
#else
    fprintf(
        stderr,
        "seccomp: WARNING: can't block execveat because __NR_execveat was not "
        "defined in the build environment\n");
#endif
    /* ideally we should also block open() and openat() but required for
     * resolv.conf */
    BLOCK(open_by_handle_at);
    BLOCK(ptrace);
    BLOCK(prctl);
    BLOCK(process_vm_readv);
    BLOCK(process_vm_writev);
    BLOCK(mount);
    BLOCK(name_to_handle_at);
    BLOCK(setns);
    BLOCK(umount);
    BLOCK(umount2);
    BLOCK(unshare);
#undef BLOCK
#undef BLOCK_ACTION
    printf(".\n");
    rc = seccomp_load(ctx);
ret:
    seccomp_release(ctx);
    return rc;
}
