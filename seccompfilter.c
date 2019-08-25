/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <seccomp.h>

int enable_seccomp()
{
    int rc = -1;
    /* Allow everything by default and block dangerous syscalls explicitly,
     * as it is hard to find the correct set of required syscalls */
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL)
        goto ret;
    printf("seccomp: The following syscalls will be blocked by seccomp:");
#ifdef SCMP_ACT_KILL_PROCESS
#define BLOCK_ACTION SCMP_ACT_KILL_PROCESS
#else
#define BLOCK_ACTION SCMP_ACT_KILL
#endif
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
    fprintf(stderr,
            "seccomp: can't block execevat because __NR_execveat was not "
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
