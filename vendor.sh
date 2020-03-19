#!/bin/bash
set -eux -o pipefail
# Mar 17, 2020 (v4.2.0)
LIBSLIRP_COMMIT=daba14c3416fa9641ab4453a9a11e7f8bde08875
LIBSLIRP_REPO=https://gitlab.freedesktop.org/slirp/libslirp.git

# Feb 21, 2020
PARSON_COMMIT=70dc239f8f54c80bf58477b25435fd3dd3102804
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
