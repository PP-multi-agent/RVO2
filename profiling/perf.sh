#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# perf stat + a call-graph sample for every workload, or for one workload
# when arguments are passed. On WSL the Ubuntu perf package often does not
# match the Microsoft kernel; this script then points at callgrind.
#
#   bash profiling/perf.sh
#   bash profiling/perf.sh circle 200 1000
#   bash profiling/perf.sh roadmap 200

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_linux

if ! command -v perf >/dev/null 2>&1; then
  echo "perf nu este instalat." >&2
  echo "Încearcă: bash profiling/install-tools.sh" >&2
  echo "Dacă kernelul WSL tot nu are perf, folosește bash profiling/callgrind.sh" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"
if ! perf stat -e task-clock -- true >"${BUILD_DIR}/perf-probe.txt" 2>&1; then
  echo "perf nu poate măsura pe kernelul ăsta." >&2
  cat "${BUILD_DIR}/perf-probe.txt" >&2
  echo "Folosește bash profiling/callgrind.sh" >&2
  exit 1
fi

bash "${_PROFILING_DIR}/build.sh"

profile_one() {
  local scenario="$1"
  local steps="${2:-200}"
  local agents="${3:-}"
  local tag="${scenario}"
  local data report
  if [[ -n "${agents}" ]]; then
    tag="${scenario}-${agents}"
  fi
  data="${BUILD_DIR}/perf-${tag}.data"
  report="${BUILD_DIR}/perf-${tag}.txt"

  echo "perf stat: ${tag}"
  if [[ -n "${agents}" ]]; then
    perf stat -r 3 -- "${BENCH}" "${scenario}" "${steps}" "${agents}"
    echo "perf record: ${tag}"
    perf record -g --call-graph fp -F 999 -o "${data}" -- \
      "${BENCH}" "${scenario}" "${steps}" "${agents}"
  else
    perf stat -r 3 -- "${BENCH}" "${scenario}" "${steps}"
    echo "perf record: ${tag}"
    perf record -g --call-graph fp -F 999 -o "${data}" -- \
      "${BENCH}" "${scenario}" "${steps}"
  fi
  perf report -i "${data}" --stdio --no-children -n >"${report}"
  head -n 25 "${report}"
  echo "Raport: ${report}"
  echo
}

if [[ $# -eq 0 ]]; then
  while read -r scenario agents; do
    [[ -z "${scenario}" ]] && continue
    profile_one "${scenario}" 200 "${agents}"
  done < <("${BENCH}" list)
else
  profile_one "$@"
fi
