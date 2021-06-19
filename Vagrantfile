Vagrant.configure("2") do |config|
  require 'etc'
  config.vm.provider "virtualbox" do |vbox|
    vbox.cpus = [1, Etc.nprocessors].max
# Change VirtualBox itself's slirp CIDR so that it doesn't conflict with slirp4netns
    vbox.customize ["modifyvm", :id, "--natnet1", "10.0.200.0/24"]
  end
  config.vm.box = "centos/7"
  config.vm.synced_folder ".", "/vagrant", disabled: true
  config.vm.synced_folder ".", "/src/slirp4netns", type: "rsync"
  config.vm.provision "shell",
    inline: <<~'SHELL'
      set -xeu
      sysctl user.max_user_namespaces=65536

      yum install -y \
        epel-release \
        https://repo.ius.io/ius-release-el7.rpm

      yum install -y \
        autoconf automake make gcc gperf libtool \
        git-core meson ninja-build \
        glib2-devel libcap-devel \
        git-core libtool iproute iputils iperf3 nmap jq

      # TODO: install udhcpc (required by test-slirp4netns-dhcp.sh)

      cd /src
      chown vagrant .

      su vagrant -c '
        set -xeu

        git clone --depth=1 --no-checkout https://github.com/seccomp/libseccomp
        git -C ./libseccomp fetch --tags --depth=1

        git clone --depth=1 --no-checkout https://gitlab.freedesktop.org/slirp/libslirp.git
        git -C ./libslirp fetch --tags --depth=1

        touch ./build-and-test
        chmod a+x ./build-and-test
      '

      cat > ./build-and-test <<'EOS'
      #! /bin/sh
      set -xeu
      src_dir='/src'

      prefix="${PREFIX:-${HOME}/prefix}"
      build_root="${BUILD_ROOT:-${prefix}/build}"
      rm -rf "${prefix}" "${build_root}"
      mkdir -p "${build_root}"

      export CFLAGS="-I${prefix}"
      export LDFLAGS="-L${prefix} -Wl,-rpath,${prefix}/lib"
      export PKG_CONFIG_PATH="${prefix}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"

      git -C "${src_dir}/libseccomp" fetch --depth=1 origin "${LIBSECCOMP_COMMIT:-v2.4.3}"
      git -C "${src_dir}/libseccomp" checkout FETCH_HEAD
      ( cd "${src_dir}/libseccomp" && ./autogen.sh )
      mkdir "${build_root}/libseccomp"
      pushd "${build_root}/libseccomp"
      "${src_dir}/libseccomp/configure" --prefix="${prefix}"
      make -j "$( nproc )" CFLAGS+="-I$( pwd )/include"
      make install
      popd

      git -C "${src_dir}/libslirp" fetch --depth=1 origin "${LIBSLIRP_COMMIT:-v4.1.0}"
      git -C "${src_dir}/libslirp" checkout FETCH_HEAD
      mkdir "${build_root}/libslirp"
      pushd "${build_root}/libslirp"
      meson setup --prefix="${prefix}" --libdir=lib . "${src_dir}/libslirp"
      ninja -C . install
      popd

      ( cd "${src_dir}/slirp4netns" && ./autogen.sh )
      mkdir "${build_root}/slirp4netns"
      pushd "${build_root}/slirp4netns"
      "${src_dir}/slirp4netns/configure" --prefix="${prefix}"
      make -j "$( nproc )"

      make ci 'CLANGTIDY=echo skipping:' 'CLANGFORMAT=echo skipping:'
      popd
      EOS
    SHELL
end
