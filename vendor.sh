#!/bin/bash
set -eux -o pipefail
# May 24, 2019
LIBSLIRP_COMMIT=113a219a69adc730ffa860c3f432049f8aa8f714
LIBSLIRP_REPO=https://gitlab.freedesktop.org/slirp/libslirp.git

# May 14, 2019
PARSON_COMMIT=33e5519d0ae68784c91c92af2f48a5b07dc14490
PARSON_REPO=https://github.com/kgabis/parson.git

# prepare
slirp4netns_root=$(realpath $(dirname $0))
tmp=$(mktemp -d /tmp/slirp4netns-vendor.XXXXXXXXXX)
tmp_git=$tmp/git
tmp_vendor=$tmp/vendor
mkdir -p $tmp_git $tmp_vendor

# vendor libslirp
git clone $LIBSLIRP_REPO $tmp_git/libslirp
(
	cd $tmp_git/libslirp
	git checkout $LIBSLIRP_COMMIT
	git am $slirp4netns_root/vendor_patches/libslirp/*.patch
	# run make to generate src/libslirp-version.h
	make
	mkdir -p $tmp_vendor/libslirp/src
	cp -a .clang-format COPYRIGHT README.md $tmp_vendor/libslirp
	cp -a src/{*.c,*.h} $tmp_vendor/libslirp/src
)

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
* libslirp: $LIBSLIRP_REPO (\`$LIBSLIRP_COMMIT\`)
* parson: $PARSON_REPO (\`$PARSON_COMMIT\`)

Applied patches (sha256sum):
\`\`\`
$(
	cd $slirp4netns_root
	sha256sum vendor_patches/*/*
)
\`\`\`

Please do not edit the contents under this directory manually.

See also [\`../vendor.md\`](../vendor.md).
EOF

# fix up
rm -rf $slirp4netns_root/vendor
mv $tmp_vendor $slirp4netns_root/vendor
rm -rf $tmp
