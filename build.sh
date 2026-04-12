#!/bin/bash
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
cmake --build --preset build-ninja-relwithdebinfo
./build_ninja/bin/neon_oubliette