#!/bin/sh

mkdir -p src/cronet/build &&
cd src/cronet/build &&
curl "https://github.com/lagenar/libcronet/releases/download/libcronet-linux-122.0.6261.111/libcronet-linux-x86_64.tar.gz" -L -o libcronet.tar.gz &&
tar -xzvf libcronet.tar.gz &&
find src -name "libcronet*" -exec mv -t . {} \; &&
mkdir include &&
find src -name "*\.h" -exec mv -t include/ {} \; &&
rm libcronet.tar.gz &&
rm -rf src