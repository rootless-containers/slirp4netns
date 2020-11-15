/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/seccomp.h>
#include <seccomp.h>
#include "seccomparch.h"

#if defined(SCMP_ACT_KILL_PROCESS) && defined(SECCOMP_GET_ACTION_AVAIL) && \
    defined(SECCOMP_RET_KILL_PROCESS)
#include <unistd.h>
#include <sys/syscall.h>

uint32_t get_block_action()
{
    const uint32_t action = SECCOMP_RET_KILL_PROCESS;
    /* Syscall fails if either actions_avail or kill_process is not available */
    if (syscall(__NR_seccomp, SECCOMP_GET_ACTION_AVAIL, 0, &action) == 0)
        return SCMP_ACT_KILL_PROCESS;
    return SCMP_ACT_KILL;
}
#else
uint32_t get_block_action()
{
    return SCMP_ACT_KILL;
}
#endif

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
    uint32_t block_action = get_block_action();
#define BLOCK(x)                                                  \
    {                                                             \
        rc = seccomp_rule_add(ctx, block_action, SCMP_SYS(x), 0); \
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
    printf(".\n");
    rc = seccomp_load(ctx);
ret:
    seccomp_release(ctx);
    return rc;
}
