#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# Configure and build rvo-bench in _build-prof.
# RelWithDebInfo keeps optimizations and debug symbols.
# Position printing stays off, so the measurement is the simulation.

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_linux

if ! cmake_new_enough "$(command -v cmake 2>/dev/null || true)"; then
  echo "Trebuie CMake 3.26 sau mai nou. Rulează: bash profiling/install-tools.sh" >&2
  exit 1
fi
if ! command -v g++ >/dev/null 2>&1; then
  echo "g++ lipsește. Rulează: bash profiling/install-tools.sh" >&2
  exit 1
fi

cmake_args=(
  -S "${ROOT}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
  -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer
  -DOUTPUT_TIME_AND_POSITIONS=OFF
)

if [[ "${OPENMP:-0}" == "1" ]]; then
  cmake_args+=(-DENABLE_OPENMP=ON)
  echo "OpenMP: pornit"
else
  cmake_args+=(-DENABLE_OPENMP=OFF)
  echo "OpenMP: oprit (setează OPENMP=1 ca să-l pornești)"
fi

cmake "${cmake_args[@]}"

if [[ "${OPENMP:-0}" == "1" ]]; then
  if ! grep -q 'FIND_PACKAGE_MESSAGE_DETAILS_OpenMP:INTERNAL=\[TRUE\]' \
      "${BUILD_DIR}/CMakeCache.txt"; then
    echo "OpenMP nu a fost găsit. Rulează: bash profiling/install-tools.sh" >&2
    exit 1
  fi
fi
cmake --build "${BUILD_DIR}" --parallel --target rvo-bench

echo "Gata: ${BENCH}"
