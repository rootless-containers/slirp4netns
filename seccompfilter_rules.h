/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SLIRP4NETNS_SECCOMPFILTER_RULES_H
#define SLIRP4NETNS_SECCOMPFILTER_RULES_H
#ifndef BLOCK
#error "Included in an unexpected way?"
#endif
BLOCK(execve);
/* ideally we should also block open() and openat() but required for resolv.conf
 */
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
#endif
