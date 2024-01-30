#!/bin/bash

g++ -I/home/lucas/chromium/src/components/cronet/out/Cronet/cronet/include main.cc sample_executor.cc sample_url_request_callback.cc -L/home/lucas/chromium/src/components/cronet/out/Cronet/ -lcronet.123.0.6270.0 -o cronet

LD_LIBRARY_PATH=/home/lucas/chromium/src/components/cronet/out/Cronet/:$LD_LIBRARY_PATH ./cronet