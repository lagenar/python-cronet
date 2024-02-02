#!/bin/bash


include_dir="$HOME/chromium/src/components/cronet/out/Cronet/cronet/include"
lib_dir="$HOME/chromium/src/components/cronet/out/Cronet/"

c++ -I"$include_dir" -O3 -Wall -shared -std=c++11 -fPIC $(python3 -m pybind11 --includes) cronet.cpp executor.cc url_request_callback.cc -L"$lib_dir" -lcronet.123.0.6270.0 -o cronet$(python3-config --extension-suffix)
LD_LIBRARY_PATH=$lib_dir:$LD_LIBRARY_PATH python -c 'import cronet; print(cronet.get("https://example.com"))'