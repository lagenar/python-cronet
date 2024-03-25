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

/app/libcronet/install_deps.sh

gclient runhooks

cd src/components/cronet
gn gen out/Cronet/ --args='target_os="linux" is_debug=false is_component_build=false'
ninja -C out/Cronet cronet_package

cp -r out/Cronet/cronet/ /app/cronet_build

cd /app
rm -rf out
