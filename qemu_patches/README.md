# `qemu_patches`

This directory contains the slirp4netns patch set for QEMU slirp.
We will propose our patch set to the QEMU slirp upstream later.

The `*.patch` files in this directory are applied to [`../qemu`](../qemu) via [`sync.sh`](./sync.sh).
Please DO NOT edit the contents under [`../qemu`](../qemu) manually.

## Update `QEMU_COMMIT`

Steps:
* Update `QEMU_COMMIT` specified in [`sync.sh`](./sync.sh).
* Rebase `*.patch` if needed (see below).
* Run [`sync.sh`](./sync.sh).

## Modify `*.patch`

Please feel free to replace/add/remove `*.patch` files in this directory!

Steps:
* Clone the upstream QEMU (`git clone https://github.com/qemu/qemu.git`)
* Checkout `QEMU_COMMIT` specified in [`sync.sh`](./sync.sh)
* Apply patches in this directory (`git am *.patch`).
* Commit your own change with `Signed-off-by` line (`git commit -a -s`). See [`https://wiki.qemu.org/Contribute/SubmitAPatch#Patch_emails_must_include_a_Signed-off-by:_line`](https://wiki.qemu.org/Contribute/SubmitAPatch#Patch_emails_must_include_a_Signed-off-by:_line).
* Consider melding your change into existing patches if your change is trivial (`git rebase -i ...`).
* Run `git format-patch upstream/master` and put the new patch set into this directory.
* Run [`sync.sh`](./sync.sh).
* Open a PR to the slirp4netns repo. 

Note: We may squash your patch to another patch but we will keep your `Signed-off-by` line.
