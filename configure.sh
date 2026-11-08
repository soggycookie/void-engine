#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build \
  -G Ninja \
  -DCMAKE_C_COMPILER="C:/Users/thean/scoop/apps/llvm/current/bin/clang.exe" \
  -DCMAKE_CXX_COMPILER="C:/Users/thean/scoop/apps/llvm/current/bin/clang++.exe" \
  -DCMAKE_RC_COMPILER="C:/Users/thean/scoop/apps/llvm/current/bin/llvm-rc.exe" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Configuration completed."
