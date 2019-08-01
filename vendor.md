# Vendor

The `*.patch` files in `vendor_patches` directory (if exists) are applied to `vendor` via [`vendor.sh`](./vendor.sh).
Please DO NOT edit files under `vendor`.

## Update vendor

Steps:
* Update commits specified in [`vendor.sh`](./vendor.sh).
* Rebase `*.patch` under `vendor_patches` if needed (see below).
* Run [`vendor.sh`](./vendor.sh).

## Modify `*.patch`

Please feel free to replace/add/remove `*.patch` files in `vendor_patches` directory.

Steps:
* Clone the upstream [libslirp](https://gitlab.freedesktop.org/slirp/libslirp) repo.
* Checkout `LIBSLIRP_COMMIT` specified in [`vendor.sh`](./vendor.sh)
* Apply patches in this directory (`git am *.patch`).
* Commit your own change with `Signed-off-by` line (`git commit -a -s`). See [`https://wiki.qemu.org/Contribute/SubmitAPatch#Patch_emails_must_include_a_Signed-off-by:_line`](https://wiki.qemu.org/Contribute/SubmitAPatch#Patch_emails_must_include_a_Signed-off-by:_line).
* Consider melding your change into existing patches if your change is trivial (`git rebase -i ...`).
* Run `git format-patch upstream/master` and put the new patch set into this directory.
* Run [`vendor.sh`](./vendor.sh).
* Open a PR to the slirp4netns repo. 

Note: We may squash your patch to another patch but we will keep your `Signed-off-by` line.
