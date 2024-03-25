#!/bin/bash

set -e

/app/libcronet/install_deps.sh

cd /app
mv /app/cronet_build /tmp
cp /tmp/cronet_build/libcronet*so /lib64
cp /tmp/cronet_build/include/*.h /usr/local/include


function repair_wheel() {
  local python_version="$1"
  local wheel_version="$2"

  python${python_version} -m pip uninstall --no-input auditwheel
  python${python_version} -m pip install git+https://github.com/lagenar/auditwheel.git
  python${python_version} -m build
  python${python_version} -m auditwheel repair --plat manylinux_2_28_x86_64 "dist/cronet-${VERSION}-cp${wheel_version}-cp${wheel_version}-linux_x86_64.whl"
}


repair_wheel "3.8" "38"
repair_wheel "3.9" "39"
repair_wheel "3.10" "310"
repair_wheel "3.11" "311"
repair_wheel "3.12" "312"


mv /tmp/cronet_build /app
