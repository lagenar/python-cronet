# Cronet

- Build: https://chromium.googlesource.com/chromium/src/+/master/components/cronet/build_instructions.md

```
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="${HOME}/depot_tools:$PATH"
mkdir ~/chromium && cd ~/chromium
fetch --nohooks --no-history chromium
cd src
./build/install-build-deps.sh
gclient runhooks
gclient sync
cd components/cronet
gn gen out/Cronet
ninja -C out/Cronet cronet_package
```

- c++ example: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/components/cronet/native/sample/main.cc
- Java API: https://chromium.googlesource.com/chromium/src/+/HEAD/components/cronet/README.md

# c++

- https://github.com/pybind/pybind11

# 2024-02-01

- Cython script that gets a url response using cronet

get
post
user-agent