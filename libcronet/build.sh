#!/bin/bash

set -e

cd /app
mkdir -p out
rm -rf out/*
cd out

git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$(pwd)/depot_tools:$PATH"
git clone -b $CHROMIUM --depth=2 https://chromium.googlesource.com/chromium/src

cd src
git apply /app/libcronet/proxy_support_*.patch
cd ..
echo 'solutions = [
      {
        "name": "src",
        "url": "https://chromium.googlesource.com/chromium/src.git",
        "managed": False,
        "custom_deps": {},
        "custom_vars": {},
      },
]' > .gclient

gclient sync --no-history --nohooks

pipx install pip
pip3 install psutil

yum -y install git bzip2 tar pkgconfig atk-devel alsa-lib-devel \
bison binutils brlapi-devel bluez-libs-devel bzip2-devel cairo-devel \
cups-devel dbus-devel dbus-glib-devel expat-devel fontconfig-devel \
freetype-devel gcc-c++ glib2-devel glibc.i686 gperf glib2-devel \
gtk3-devel java-1.*.0-openjdk-devel libatomic libcap-devel libffi-devel \
libgcc.i686 libjpeg-devel libstdc++.i686 libX11-devel libXScrnSaver-devel \
libXtst-devel libxkbcommon-x11-devel ncurses-compat-libs nspr-devel nss-devel \
pam-devel pango-devel pciutils-devel pulseaudio-libs-devel zlib.i686 httpd \
mod_ssl php php-cli xorg-x11-server-Xvfb

gclient runhooks

cd src/components/cronet
gn gen out/Cronet/ --args='target_os="linux" is_debug=false is_component_build=false'
ninja -C out/Cronet cronet_package

cp out/Cronet/cronet/libcronet*so /lib64
cp out/Cronet/cronet/include/*.h /usr/local/include

cd /app

python3.8 -m build
auditwheel repair --plat manylinux_2_28_x86_64 dist/cronet-$VERSION-cp38-cp38-linux_x86_64.whl

python3.9 -m build
auditwheel repair --plat manylinux_2_28_x86_64 dist/cronet-$VERSION-cp39-cp39-linux_x86_64.whl

python3.10 -m build
auditwheel repair --plat manylinux_2_28_x86_64 dist/cronet-$VERSION-cp310-cp310-linux_x86_64.whl

python3.11 -m build
auditwheel repair --plat manylinux_2_28_x86_64 dist/cronet-$VERSION-cp311-cp311-linux_x86_64.whl

python3.12 -m build
auditwheel repair --plat manylinux_2_28_x86_64 dist/cronet-$VERSION-cp312-cp312-linux_x86_64.whl
