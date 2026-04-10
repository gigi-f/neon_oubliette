#!/bin/bash
PROJECT_ROOT="/Users/gm1/Code/neon_oubliette"
BUILD_DIR="${PROJECT_ROOT}/build_ninja_iwyu"
IWYU_TOOL="/opt/homebrew/bin/iwyu_tool.py"

# Ensure build dir exists and is configured
if [ ! -d "${BUILD_DIR}" ]; then
    echo "==> Configuring IWYU build..."
    cmake --preset ninja-iwyu
fi

echo "==> Running IWYU analysis..."
# We use the build preset which has CMAKE_CXX_INCLUDE_WHAT_YOU_USE set,
# but we can also run iwyu_tool.py directly for full project report.
if [ -f "${BUILD_DIR}/compile_commands.json" ]; then
    "${IWYU_TOOL}" -p "${BUILD_DIR}" -- -Xiwyu --mapping_file="${PROJECT_ROOT}/iwyu.imp"
else
    echo "Error: compile_commands.json not found in ${BUILD_DIR}. Run cmake --preset ninja-iwyu first."
    exit 1
fi
