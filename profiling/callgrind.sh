#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# Function-level profile with valgrind/callgrind on the OpenMP build.
# The default run is the same 12 workloads as bench.sh, 200 steps, once
# each: first one thread, then every logical CPU. There are no repeats.
# Callgrind counts instructions; the parallel speedup is in bench.txt.
#
#   bash profiling/callgrind.sh
#   bash profiling/callgrind.sh circle 200 1000
#   bash profiling/callgrind.sh roadmap 200

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_linux

if ! command -v valgrind >/dev/null 2>&1; then
  echo "valgrind lipsește. Rulează: bash profiling/install-tools.sh" >&2
  exit 1
fi

OPENMP=1 bash "${_PROFILING_DIR}/build.sh"

prepare_results_dir
explain_results_dir

logical="$(nproc)"

profile_one() {
  local scenario="$1"
  local steps="$2"
  local agents="$3"
  local thread_count="$4"
  local tag="${scenario}"
  local out report bench_out got
  if [[ -n "${agents}" ]]; then
    tag="${scenario}-${agents}"
  fi
  out="${BUILD_DIR}/callgrind-${tag}-threads-${thread_count}.out"
  bench_out="${BUILD_DIR}/callgrind-${tag}-threads-${thread_count}.bench"
  report="${RESULTS_DIR}/callgrind-${tag}-threads-${thread_count}.txt"

  export OMP_NUM_THREADS="${thread_count}"
  echo "Callgrind: ${scenario} pași=${steps} agenți=${agents:-toate nivelele} threads=${thread_count}"
  echo "Durează mult mai mult decât o rulare normală."
  if [[ -n "${agents}" ]]; then
    valgrind --tool=callgrind --callgrind-out-file="${out}" \
      "${BENCH}" "${scenario}" "${steps}" "${agents}" >"${bench_out}"
  else
    valgrind --tool=callgrind --callgrind-out-file="${out}" \
      "${BENCH}" "${scenario}" "${steps}" >"${bench_out}"
  fi
  got="$(sed -n 's/.*threads=\([0-9][0-9]*\).*/\1/p' "${bench_out}" | sort -u)"
  if [[ "${got}" != "${thread_count}" ]]; then
    echo "Rularea raportează threads=${got:-lipsă}, așteptam ${thread_count}." >&2
    echo "OpenMP nu este activ în binar. Rulează bash profiling/install-tools.sh" >&2
    exit 1
  fi
  {
    echo "scenario=${scenario} agents=${agents:-all} steps=${steps} threads=${thread_count}"
    echo
    callgrind_annotate "${out}"
  } >"${report}"
  echo "--- ${tag} threads=${thread_count} ---"
  head -n 20 "${report}"
  echo "Raport: ${report}"
  echo
}

run_both() {
  local scenario="$1"
  local steps="$2"
  local agents="${3:-}"
  profile_one "${scenario}" "${steps}" "${agents}" 1
  if [[ "${logical}" -gt 1 ]]; then
    profile_one "${scenario}" "${steps}" "${agents}" "${logical}"
  fi
}

if [[ $# -eq 0 ]]; then
  echo "Callgrind: aceleași 12 sarcini ca bench, 200 de pași, o singură dată."
  echo "threads=1, apoi threads=${logical}."
  while read -r scenario agents; do
    [[ -z "${scenario}" ]] && continue
    run_both "${scenario}" 200 "${agents}"
  done < <("${BENCH}" list)
else
  run_both "${1}" "${2:-200}" "${3:-}"
fi

echo "Caută computeNewVelocity, insertAgentNeighbor, Frame::queryRecursive, Frame::build."
echo "La roadmap apare și queryVisibility."
echo "threads= este pe prima linie a fiecărui raport. Accelerația este în bench.txt."
