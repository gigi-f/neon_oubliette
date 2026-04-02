#!/bin/bash
cd /Users/gm1/Code/ag2_workspace/neon_oubliette/build
cmake ..
make -j$(sysctl -n hw.ncpu)
