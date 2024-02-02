#!/bin/bash

lib_dir="$HOME/chromium/src/components/cronet/out/Cronet/"
LD_LIBRARY_PATH=$lib_dir:$LD_LIBRARY_PATH python -i test.py  
