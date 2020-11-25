/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SLIRP4NETNS_SECCOMPFILTER_RULES_H
#define SLIRP4NETNS_SECCOMPFILTER_RULES_H
#ifndef BLOCK
#error "Included in an unexpected way?"
#endif
/*
  NOTE:
  - Run `sudo systemd-analyze syscall-filter` to show list of syscall groups.
  - Ideally we should also block open() and openat(), but these calls are required for opening resolv.conf
 */

/* group: @default */
BLOCK(execve);

/* group: @debug */
BLOCK(lookup_dcookie);
BLOCK(pidfd_getfd);
BLOCK(ptrace);

/* group: @ipc */
BLOCK(process_vm_readv);
BLOCK(process_vm_writev);

/* group: @module*/
BLOCK(delete_module);
BLOCK(finit_module);
BLOCK(init_module);

/* group: @mount */
BLOCK(chroot);
BLOCK(fsconfig);
BLOCK(fsmount);
BLOCK(fsopen);
BLOCK(fspick);
BLOCK(mount);
BLOCK(move_mount);
BLOCK(open_tree);
BLOCK(pivot_root);
BLOCK(umount);
BLOCK(umount2);

/* group: @privileged */
BLOCK(open_by_handle_at);

/* group: @process */
BLOCK(execveat);
BLOCK(pidfd_open);
BLOCK(pidfd_send_signal);
BLOCK(prctl);
BLOCK(setns);
BLOCK(unshare);

/* group: @reboot */
BLOCK(kexec_file_load);
BLOCK(kexec_load);
BLOCK(reboot);

/* group: @system-service */
BLOCK(name_to_handle_at);

#endif
