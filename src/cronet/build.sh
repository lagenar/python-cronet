#!/bin/bash


include_dir="$HOME/cronet/cronet/include"
lib_dir="$HOME/cronet/"

gcc -I"$include_dir" -I"$HOME/.pyenv/versions/3.10.13/include/python3.10" --shared -fvisibility=hidden -O3 -Wall -fPIC  _cronet.c -L"$lib_dir" -lcronet.123.0.6300.0 -L "$HOME/.pyenv/versions/3.10.13/lib/" -lpython3.10 -o _cronet$(python3-config --extension-suffix)
