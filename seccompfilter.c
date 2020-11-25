/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/seccomp.h>
#include <glib.h>
#include <seccomp.h>
#include "seccomparch.h"

#if defined(SCMP_ACT_KILL_PROCESS) && defined(SECCOMP_GET_ACTION_AVAIL) && \
    defined(SECCOMP_RET_KILL_PROCESS)
#include <unistd.h>
#include <sys/syscall.h>

static uint32_t get_block_action()
{
    const uint32_t action = SECCOMP_RET_KILL_PROCESS;
    /* Syscall fails if either actions_avail or kill_process is not available */
    if (syscall(__NR_seccomp, SECCOMP_GET_ACTION_AVAIL, 0, &action) == 0)
        return SCMP_ACT_KILL_PROCESS;
    return SCMP_ACT_KILL;
}
#else
static uint32_t get_block_action()
{
    return SCMP_ACT_KILL;
}
#endif

static void add_block_rule(scmp_filter_ctx ctx, const char *name,
                           uint32_t block_action, GString *blocked,
                           GString *skipped_undefined, GString *skipped_failed)
{
    int rc = -1, num = seccomp_syscall_resolve_name(name);
    if (num == __NR_SCMP_ERROR) {
        g_string_append_printf(skipped_undefined, " %s", name);
        return;
    }
    if ((rc = seccomp_rule_add(ctx, block_action, num, 0)) != 0) {
        g_string_append_printf(skipped_failed, " %s(%s)", name, strerror(-rc));
        return;
    }
    g_string_append_printf(blocked, " %s", name);
}

int enable_seccomp()
{
    int rc = -1, i;
    uint32_t block_action = get_block_action();
    GString *blocked = g_string_new(NULL);
    GString *skipped_undefined = g_string_new(NULL);
    GString *skipped_failed = g_string_new(NULL);
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
#define BLOCK(x)                                                      \
    add_block_rule(ctx, #x, block_action, blocked, skipped_undefined, \
                   skipped_failed)
#include "seccompfilter_rules.h"
#undef BLOCK
    if ((rc = seccomp_load(ctx)) != 0) {
        fprintf(stderr, "seccomp: seccomp_load(): %s\n", strerror(-rc));
        goto ret;
    }
    printf("seccomp: The following syscalls are blocked:%s\n", blocked->str);
    if (skipped_undefined->len > 0) {
        fprintf(stderr,
                "seccomp: WARNING: the following syscalls are not defined "
                "in libseccomp and cannot be "
                "blocked:%s\n",
                skipped_undefined->str);
    }
    if (skipped_failed->len > 0) {
        fprintf(stderr,
                "seccomp: WARNING: the following syscalls cannot be "
                "blocked due to unexpected errors:%s\n",
                skipped_failed->str);
    }
ret:
    seccomp_release(ctx);
    g_string_free(blocked, TRUE);
    g_string_free(skipped_undefined, TRUE);
    g_string_free(skipped_failed, TRUE);
    return rc;
}
