#!/bin/bash
set -eux -o pipefail
QEMU_REPO=https://github.com/qemu/qemu.git
# v4.0.0-rc4 (April 2019)
QEMU_COMMIT=eeba63fc7fface36f438bcbc0d3b02e7dcb59983
cd $(dirname $0)/..
slirp4netns_dir=$(pwd)
slirp4netns_qemu_dir=$slirp4netns_dir/qemu
if ! [ -w $HOME ]; then
	echo "HOME needs to be set and writable"
	exit 1
fi
qemu_dir=$HOME/.cache/slirp4netns-qemu

fetch_qemu() {
	(
		cd $qemu_dir
		# TODO: cache
		rm -rf .git *
		git init
		git remote add origin $QEMU_REPO
		git fetch --depth 1 origin $QEMU_COMMIT
		git checkout FETCH_HEAD
	)
}

mkdir -p $qemu_dir
(
	cd $qemu_dir
	git checkout $QEMU_COMMIT || fetch_qemu
	git am $slirp4netns_dir/qemu_patches/*.patch
)

rm -rf $slirp4netns_qemu_dir
mkdir -p $slirp4netns_qemu_dir
cp -a $qemu_dir/{COPYING*,LICENSE*,slirp} $slirp4netns_qemu_dir
rm -f $slirp4netns_qemu_dir/slirp/src/Makefile
cat << EOF > $slirp4netns_qemu_dir/README.md
# DO NOT EDIT MANUALLY

This directory was synced from QEMU \`${QEMU_COMMIT}\` (\`${QEMU_REPO}\`),
with the following patches (sha256sum):
\`\`\`
$(cd $slirp4netns_dir/qemu_patches; sha256sum *.patch)
\`\`\`

Please do not edit the contents under this directory manually.

See also [\`../qemu_patches/README.md\`](../qemu_patches/README.md).
EOF
