#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# Time every workload twice: one thread, then every logical CPU.
# The full suite is saved to bench-results/<commit>/<machine>/bench.txt
# and ends with a score. A partial run stays in _build-prof/bench.txt.
#
#   bash profiling/bench.sh
#   bash profiling/bench.sh circle 200
#   bash profiling/bench.sh blocks 200 400
#   REPEATS=3 bash profiling/bench.sh circle 200 100

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_linux

OPENMP=1 bash "${_PROFILING_DIR}/build.sh"

full_suite=0
if [[ $# -eq 0 || "${1}" == "all" ]]; then
  full_suite=1
fi
if [[ $# -eq 0 ]]; then
  set -- all
fi

repeats="${REPEATS:-10}"
if ! [[ "${repeats}" =~ ^[1-9][0-9]*$ ]]; then
  echo "REPEATS trebuie să fie un întreg pozitiv." >&2
  exit 1
fi

select_results_dir
if [[ "${full_suite}" -eq 1 ]]; then
  mkdir -p "${RESULTS_DIR}"
  explain_results_dir
  report="${RESULTS_DIR}/bench.txt"
  suite_name="full"
else
  echo "Rulare parțială: fără scor și fără scriere în bench-results."
  report="${BUILD_DIR}/bench.txt"
  suite_name="partial"
fi
raw="${BUILD_DIR}/bench-last.txt"
mkdir -p "${BUILD_DIR}"
: >"${report}"

log() {
  printf '%s\n' "$*" | tee -a "${report}"
}

field() {
  local file="$1"
  local key="$2"
  awk -F: -v key="${key}" '
    $1 ~ "^" key "[ \t]*$" {
      gsub(/^[ \t]+|[ \t]+$/, "", $2)
      print $2
      exit
    }
  ' "${file}"
}

cpu_model="$(field /proc/cpuinfo "model name" || true)"
if [[ -z "${cpu_model}" ]]; then
  cpu_model="$(field /proc/cpuinfo "Model" || true)"
fi
if [[ -z "${cpu_model}" ]]; then
  cpu_model="unknown"
fi
cpu_cores="$(field /proc/cpuinfo "cpu cores" || true)"
siblings="$(field /proc/cpuinfo "siblings" || true)"
sockets="$(awk -F: '/^physical id[ \t]*:/ { gsub(/^[ \t]+/, "", $2); print $2 }' /proc/cpuinfo | sort -u | wc -l | tr -d ' ')"
if [[ -z "${sockets}" || "${sockets}" == "0" ]]; then
  sockets="unknown"
fi
threads_per_core="unknown"
if [[ -n "${cpu_cores}" && -n "${siblings}" && "${cpu_cores}" != "0" ]]; then
  threads_per_core="$((siblings / cpu_cores))"
fi
logical="$(nproc)"
mem_kib="$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo)"
os_name="$(awk -F= '$1 == "PRETTY_NAME" { gsub(/"/, "", $2); print $2 }' /etc/os-release)"

{
  echo "date=$(date -Is)"
  echo "os=${os_name}"
  echo "arch=$(uname -m)"
  echo "cpu=${cpu_model}"
  echo "sockets=${sockets}"
  echo "cores_per_socket=${cpu_cores:-unknown}"
  echo "threads_per_core=${threads_per_core}"
  echo "logical_cpus=${logical}"
  echo "memory_mib=$((mem_kib / 1024))"
  echo "compiler=$(g++ --version | head -n 1)"
  echo "openmp=on"
  echo "repeats=${repeats}"
  echo "suite=${suite_name}"
  echo "head=${RESULT_HEAD}"
  echo "results_commit=${RESULT_COMMIT}"
  echo "worktree=${RESULT_STATE}"
  echo "machine=${RESULT_MACHINE}"
  echo "command=rvo-bench $*"
} >>"${report}"

cat "${report}"

declare -A bucket
declare -A seen
declare -A checksum
keys=()

reset_samples() {
  bucket=()
  seen=()
  keys=()
}

note_line() {
  local line="$1"
  local scenario agents sample key sum
  scenario="$(printf '%s\n' "${line}" | sed -n 's/.*scenario=\([^ ]*\).*/\1/p')"
  agents="$(printf '%s\n' "${line}" | sed -n 's/.*agents=\([0-9][0-9]*\).*/\1/p')"
  sample="$(printf '%s\n' "${line}" | sed -n 's/.*per_step_us=\([0-9.][0-9.]*\).*/\1/p')"
  if [[ -z "${scenario}" || -z "${agents}" || -z "${sample}" ]]; then
    echo "Nu am putut citi o linie de rezultat: ${line}" >&2
    exit 1
  fi
  key="${scenario} ${agents}"
  if [[ -z "${seen[${key}]:-}" ]]; then
    seen["${key}"]=1
    keys+=("${key}")
  fi
  bucket["${key}"]+="${sample}"$'\n'

  # Aceeași sarcină trebuie să ajungă la aceleași poziții la orice rulare și
  # cu orice număr de threaduri.
  sum="$(printf '%s\n' "${line}" | sed -n 's/.*checksum=\([0-9a-f][0-9a-f]*\).*/\1/p')"
  if [[ -z "${sum}" ]]; then
    return
  fi
  if [[ -z "${checksum[${key}]:-}" ]]; then
    checksum["${key}"]="${sum}"
  elif [[ "${checksum[${key}]}" != "${sum}" ]]; then
    echo "Pozițiile finale diferă la ${key}: checksum ${checksum[${key}]} și ${sum}." >&2
    echo "Rularea paralelă nu mai dă rezultatul rulării pe un thread." >&2
    exit 1
  fi
}

run_threads() {
  local threads="$1"
  shift
  local run line got
  export OMP_NUM_THREADS="${threads}"
  reset_samples
  log ""
  log "threads=${threads}"

  for ((run = 1; run <= repeats; ++run)); do
    if ! "${BENCH}" "$@" >"${raw}"; then
      echo "rvo-bench a eșuat." >&2
      exit 1
    fi
    got="$(sed -n 's/.*threads=\([0-9][0-9]*\).*/\1/p' "${raw}" | head -n 1)"
    if [[ "${got}" != "${threads}" ]]; then
      echo "Rularea raportează threads=${got}, așteptam ${threads}." >&2
      echo "OpenMP nu este activ în binar. Rulează bash profiling/install-tools.sh" >&2
      exit 1
    fi
    while IFS= read -r line; do
      [[ -z "${line}" ]] && continue
      log "${line}"
      note_line "${line}"
    done <"${raw}"
  done

  log ""
  log "median threads=${threads}"
  for key in "${keys[@]}"; do
    printf '%s' "${bucket[${key}]}" | sed '/^$/d' | sort -n | awk -v key="${key}" -v threads="${threads}" '
      { a[NR] = $1 }
      END {
        if (NR == 0) {
          exit 1
        }
        if (NR % 2 == 1) {
          median = a[(NR + 1) / 2]
        } else {
          median = (a[NR / 2] + a[NR / 2 + 1]) / 2
        }
        printf "%s threads=%s per_step_us min=%.3f median=%.3f max=%.3f\n", key, threads, a[1], median, a[NR]
      }
    ' | tee -a "${report}"
  done
}

run_threads 1 "$@"
if [[ "${logical}" -gt 1 ]]; then
  run_threads "${logical}" "$@"
else
  log ""
  log "logical_cpus=1, so the all-thread run is the same as threads=1"
fi

log ""
log "checksum: identical final positions in every run of each workload"

if [[ "${full_suite}" -eq 1 && "${logical}" -gt 1 ]]; then
  # Efficiency is (one-thread median / parallel median) / logical CPUs.
  # Linear scaling is 1 on any machine. overall is the geometric mean.
  expected="$("${BENCH}" list | sed '/^$/d' | wc -l | tr -d ' ')"
  speedups="$(awk -v logical="${logical}" -v expected="${expected}" '
    /per_step_us/ && /median=/ {
      threads = $0
      sub(/.*threads=/, "", threads)
      sub(/ .*/, "", threads)
      median = $0
      sub(/.*median=/, "", median)
      sub(/ .*/, "", median)
      if (median + 0 <= 0) {
        printf "median nepozitivă pentru %s\n", $1 " " $2 > "/dev/stderr"
        exit 1
      }
      key = $1 " " $2
      if (!(key in seen)) {
        seen[key] = 1
        order[++norder] = key
      }
      value[key, threads] = median + 0
      count[threads]++
    }
    END {
      if (count[1] != expected + 0 || count[logical] != expected + 0) {
        printf "medianele nu acoperă suita: threads=1 are %d, threads=%s are %d, așteptam %s\n", count[1], logical, count[logical], expected > "/dev/stderr"
        exit 1
      }
      sum = 0
      for (i = 1; i <= norder; ++i) {
        key = order[i]
        ratio = value[key, 1] / value[key, logical] / (logical + 0)
        printf "%s %.3f\n", key, ratio
        sum += log(ratio)
      }
      printf "overall %.3f\n", exp(sum / norder)
    }
  ' "${report}")"
  log ""
  log "Efficiency"
  printf '%s\n' "${speedups}" | tee -a "${report}"
fi

echo
if [[ "${full_suite}" -eq 1 ]]; then
  echo "Raport: ${report}"
else
  echo "Raport local: ${report}"
  echo "Suita completă, cu efficiency, se salvează în ${RESULTS_DIR}/bench.txt"
fi
