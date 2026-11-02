#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# Compiler, CMake, valgrind, and OpenMP (libomp). perf is attempted
# afterwards; on WSL it may be unusable because the kernel package does not
# match, and callgrind.sh is the fallback.

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_linux

sudo apt-get update
sudo apt-get install -y build-essential g++ curl ca-certificates valgrind libomp-dev

if ! cmake_new_enough "$(command -v cmake 2>/dev/null || true)"; then
  arch="$(uname -m)"
  case "${arch}" in
    x86_64) asset="cmake-3.31.8-linux-x86_64.tar.gz" ;;
    aarch64) asset="cmake-3.31.8-linux-aarch64.tar.gz" ;;
    *)
      echo "Nu am un binar CMake pentru arhitectura ${arch}." >&2
      echo "Instalează CMake 3.26 sau mai nou și rerulează." >&2
      exit 1
      ;;
  esac
  tmp="$(mktemp -d)"
  url="https://github.com/Kitware/CMake/releases/download/v3.31.8/${asset}"
  echo "CMake din apt este prea vechi. Instalez 3.31.8 în ${HOME}/.local"
  curl -fL --retry 3 -o "${tmp}/cmake.tgz" "${url}"
  tar -xzf "${tmp}/cmake.tgz" -C "${tmp}"
  mkdir -p "${HOME}/.local/bin" "${HOME}/.local/share"
  cp -a "${tmp}"/cmake-*/bin/. "${HOME}/.local/bin/"
  cp -a "${tmp}"/cmake-*/share/. "${HOME}/.local/share/"
  rm -rf "${tmp}"
  export PATH="${HOME}/.local/bin:${PATH}"
fi

if ! sudo apt-get install -y linux-tools-common linux-tools-generic; then
  echo "Pachetul perf nu s-a instalat. bash profiling/callgrind.sh rămâne disponibil."
fi

echo
echo "g++: $(g++ --version | head -n 1)"
echo "cmake: $(cmake --version | head -n 1)"
echo "valgrind: $(valgrind --version)"
if command -v perf >/dev/null 2>&1; then
  perf --version || true
else
  echo "perf: indisponibil. Folosește bash profiling/callgrind.sh"
fi
