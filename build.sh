#!/bin/bash
mkdir -p /Users/gm1/Code/neon_oubliette/build
cd /Users/gm1/Code/neon_oubliette/build
cmake ..
make -j$(sysctl -n hw.ncpu)
