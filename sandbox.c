/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/capability.h>
#include <sys/stat.h>

static int add_mount(const char *from, const char *to)
{
    int ret;

    ret = mount("", from, "", MS_SLAVE | MS_REC, NULL);
    if (ret < 0 && errno != EINVAL) {
        fprintf(stderr, "cannot make mount propagation slave %s\n", from);
        return ret;
    }
    ret = mount(from, to, "",
                MS_BIND | MS_REC | MS_SLAVE | MS_NOSUID | MS_NODEV | MS_NOEXEC,
                NULL);
    if (ret < 0) {
        fprintf(stderr, "cannot bind mount %s to %s\n", from, to);
        return ret;
    }
    ret = mount("", to, "", MS_SLAVE | MS_REC, NULL);
    if (ret < 0) {
        fprintf(stderr, "cannot make mount propagation slave %s\n", to);
        return ret;
    }
    ret = mount(from, to, "",
                MS_REMOUNT | MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV |
                    MS_NOEXEC,
                NULL);
    if (ret < 0) {
        fprintf(stderr, "cannot remount ro %s\n", to);
        return ret;
    }
    return 0;
}

/* lock down the process doing the following:
 - create a new mount namespace
 - bind mount /etc and /run from the host
 - pivot_root in the new tmpfs.
 - drop all capabilities.
*/
int create_sandbox()
{
    int ret, i;
    struct __user_cap_header_struct hdr = { _LINUX_CAPABILITY_VERSION_3, 0 };
    struct __user_cap_data_struct data[2] = { { 0 } };

    ret = unshare(CLONE_NEWNS);
    if (ret < 0) {
        fprintf(stderr, "cannot unshare new mount namespace\n");
        return ret;
    }
    ret = mount("", "/", "", MS_PRIVATE, NULL);
    if (ret < 0) {
        fprintf(stderr, "cannot remount / private\n");
        return ret;
    }

    ret = mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                "size=1k");
    if (ret < 0) {
        fprintf(stderr, "cannot mount tmpfs on /tmp\n");
        return ret;
    }

    ret = mkdir("/tmp/etc", 0755);
    if (ret < 0) {
        fprintf(stderr, "cannot mkdir /etc\n");
        return ret;
    }

    ret = mkdir("/tmp/old", 0755);
    if (ret < 0) {
        fprintf(stderr, "cannot mkdir /old\n");
        return ret;
    }

    ret = mkdir("/tmp/run", 0755);
    if (ret < 0) {
        fprintf(stderr, "cannot mkdir /run\n");
        return ret;
    }

    ret = add_mount("/etc", "/tmp/etc");
    if (ret < 0) {
        return ret;
    }

    ret = add_mount("/run", "/tmp/run");
    if (ret < 0) {
        return ret;
    }

    ret = chdir("/tmp");
    if (ret < 0) {
        fprintf(stderr, "cannot chdir to /tmp\n");
        return ret;
    }

    ret = syscall(__NR_pivot_root, ".", "old");
    if (ret < 0) {
        fprintf(stderr, "cannot pivot_root to /tmp\n");
        return ret;
    }

    ret = chdir("/");
    if (ret < 0) {
        fprintf(stderr, "cannot chdir to /\n");
        return ret;
    }

    ret = umount2("/old", MNT_DETACH);
    if (ret < 0) {
        fprintf(stderr, "cannot umount /old\n");
        return ret;
    }

    ret = rmdir("/old");
    if (ret < 0) {
        fprintf(stderr, "cannot rmdir /old\n");
        return ret;
    }

    ret = mount("tmpfs", "/", "tmpfs", MS_REMOUNT | MS_RDONLY, "size=0k");
    if (ret < 0) {
        fprintf(stderr, "cannot mount tmpfs on /tmp\n");
        return ret;
    }

    ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "prctl(PR_SET_NO_NEW_PRIVS)\n");
        return ret;
    }

    ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "prctl(PR_CAP_AMBIENT_CLEAR_ALL)\n");
        return ret;
    }
    for (i = 0;; i++) {
        if (i == CAP_NET_BIND_SERVICE)
            continue;
        ret = prctl(PR_CAPBSET_DROP, i, 0, 0, 0);
        if (ret < 0) {
            if (errno == EINVAL)
                break;
            fprintf(stderr, "prctl(PR_CAPBSET_DROP)\n");
            return ret;
        }
    }

    memset(&data, 0, sizeof(data));
    data[0].effective |= 1 << CAP_NET_BIND_SERVICE;
    data[0].permitted |= 1 << CAP_NET_BIND_SERVICE;
    data[0].inheritable |= 1 << CAP_NET_BIND_SERVICE;
    ret = capset(&hdr, data);
    if (ret < 0) {
        fprintf(stderr, "capset(0)\n");
        return ret;
    }

    return 0;
}
