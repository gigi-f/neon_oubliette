#!/bin/bash
set -e

PROJECT_ROOT="/Users/gm1/Code/neon_oubliette"
export PATH="/opt/homebrew/bin:$PATH"
BUILD_DIR="${PROJECT_ROOT}/build_ninja_trace"

REPORT_DIR="${PROJECT_ROOT}/build_reports"
CBA_BIN="/opt/homebrew/bin/ClangBuildAnalyzer"

mkdir -p "${REPORT_DIR}"

echo "==> Configuring build with time-trace..."
cmake --preset ninja-time-trace

echo "==> Building and capturing trace data..."
cmake --build --preset build-ninja-time-trace --clean-first

echo "==> Running ClangBuildAnalyzer --full..."
"${CBA_BIN}" --full "${BUILD_DIR}" "${REPORT_DIR}/cba_capture.bin"

echo "==> Generating Analysis Report..."
"${CBA_BIN}" --analyze "${REPORT_DIR}/cba_capture.bin" > "${REPORT_DIR}/build_analysis_report.txt"

echo "==> Done! Report generated at: ${REPORT_DIR}/build_analysis_report.txt"
cat "${REPORT_DIR}/build_analysis_report.txt" | head -n 50
