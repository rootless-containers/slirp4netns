#!/bin/bash
set -eux -o pipefail
# Aug 1, 2019
LIBSLIRP_COMMIT=30804efc8f80f43d415057f3099c2894b0f947c4
LIBSLIRP_REPO=https://gitlab.freedesktop.org/slirp/libslirp.git

# Jul 12, 2019
PARSON_COMMIT=c5bb9557fe98367aa8e041c65863909f12ee76b2
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
	if ls $slirp4netns_root/vendor_patches/libslirp/*.patch >/dev/null; then
		git am $slirp4netns_root/vendor_patches/libslirp/*.patch
	fi
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

EOF

if ls $slirp4netns_root/vendor_patches/libslirp/*.patch >/dev/null; then
	cat <<EOF >>$tmp_vendor/README.md
Applied patches (sha256sum):
\`\`\`
$(
		cd $slirp4netns_root
		sha256sum vendor_patches/*/*
	)
\`\`\`

EOF
fi

cat <<EOF >>$tmp_vendor/README.md
Please do not edit the contents under this directory manually.

See also [\`../vendor.md\`](../vendor.md).
EOF

# fix up
rm -rf $slirp4netns_root/vendor
mv $tmp_vendor $slirp4netns_root/vendor
rm -rf $tmp
