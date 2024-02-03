#!/bin/bash


include_dir="$HOME/chromium/src/components/cronet/out/Cronet/cronet/include"
lib_dir="$HOME/chromium/src/components/cronet/out/Cronet/"

c++ -I"$include_dir" -fvisibility=hidden -O3 -Wall -shared -std=c++11 -fPIC $(python3 -m pybind11 --includes) cronet.cc executor.cc url_request_callback.cc -L"$lib_dir" -lcronet.123.0.6270.0 -o cronet$(python3-config --extension-suffix)
