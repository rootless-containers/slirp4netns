#!/bin/bash
set -eux -o pipefail

# v1.1.3 (May 26, 2021)
PARSON_COMMIT=2d7b3ddf1280bf7f5ad82d26e09252240e7c4557
PARSON_REPO=https://github.com/kgabis/parson.git

# prepare
slirp4netns_root=$(realpath $(dirname $0))
tmp=$(mktemp -d /tmp/slirp4netns-vendor.XXXXXXXXXX)
tmp_git=$tmp/git
tmp_vendor=$tmp/vendor
mkdir -p $tmp_git $tmp_vendor

# vendor parson
git clone $PARSON_REPO $tmp_git/parson
(
	cd $tmp_git/parson
	git checkout $PARSON_COMMIT
	mkdir -p $tmp_vendor/parson
	cp -a LICENSE README.md parson.c parson.h $tmp_vendor/parson
)

# write vendor/README.md
cat <<EOF >$tmp_vendor/README.md
# DO NOT EDIT MANUALLY

Vendored components:
* parson: $PARSON_REPO (\`$PARSON_COMMIT\`)

EOF

cat <<EOF >>$tmp_vendor/README.md
Please do not edit the contents under this directory manually.

Use [\`../vendor.sh\`](../vendor.sh) to update the contents.
EOF

# fix up
rm -rf $slirp4netns_root/vendor
mv $tmp_vendor $slirp4netns_root/vendor
rm -rf $tmp
