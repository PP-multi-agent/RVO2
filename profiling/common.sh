# SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
# SPDX-License-Identifier: Apache-2.0

# Shared paths for the profiling scripts. Source this file; do not run it.

_PROFILING_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${_PROFILING_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/_build-prof"
BENCH="${BUILD_DIR}/profiling/rvo-bench"

require_linux() {
  if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Rulează scriptul ăsta într-un terminal Ubuntu/WSL, nu în PowerShell." >&2
    echo "Din PowerShell: wsl" >&2
    exit 1
  fi
}

cmake_new_enough() {
  local bin="$1"
  local ver
  [[ -n "${bin}" && -x "${bin}" ]] || return 1
  ver="$("${bin}" --version | awk 'NR==1 { print $3 }')"
  printf '%s\n%s\n' "3.26.0" "${ver}" | sort -C -V
}

# Ubuntu 22.04 and Debian 12 ship a CMake older than 3.26. install-tools.sh
# puts a newer one in ~/.local/bin. Prefer that when the system one is too old.
if ! cmake_new_enough "$(command -v cmake 2>/dev/null || true)"; then
  if [[ -x "${HOME}/.local/bin/cmake" ]]; then
    export PATH="${HOME}/.local/bin:${PATH}"
  fi
fi

slug_token() {
  printf '%s' "$1" | tr '[:upper:]' '[:lower:]' \
    | sed -e 's/(r)//g' -e 's/(tm)//g' -e 's/[^a-z0-9]\+/-/g' -e 's/^-//' -e 's/-$//'
}

machine_dir_name() {
  local host cpu
  host="$(hostname 2>/dev/null || echo unknown-host)"
  cpu="$(awk -F: '/^model name[ \t]*:/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2; exit }' /proc/cpuinfo)"
  if [[ -z "${cpu}" ]]; then
    cpu="$(awk -F: '/^Model[ \t]*:/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2; exit }' /proc/cpuinfo)"
  fi
  [[ -n "${cpu}" ]] || cpu="unknown-cpu"
  printf '%s_%s' "$(slug_token "${host}")" "$(slug_token "${cpu}")"
}

require_git() {
  if ! command -v git >/dev/null 2>&1; then
    echo "git lipsește din terminalul Linux." >&2
    exit 1
  fi
  if ! git -C "${ROOT}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Folderul proiectului nu este un repo git." >&2
    exit 1
  fi
}

# bench-results itself must not count, or saving a report would look like a
# source change and the next run would pick a different commit.
worktree_is_dirty() {
  local status
  status="$(git -C "${ROOT}" status --porcelain --untracked-files=all -- . ':(exclude)bench-results')"
  [[ -n "${status}" ]]
}

# Clean tree: the parent commit, so a run of the unchanged code stays with
# the previous snapshot. Local edits: the current commit. Same machine and
# same commit overwrites that directory.
select_results_dir() {
  local parent
  require_git
  RESULT_HEAD="$(git -C "${ROOT}" rev-parse --short=12 HEAD)"
  if worktree_is_dirty; then
    RESULT_STATE="dirty"
    RESULT_COMMIT="${RESULT_HEAD}"
    RESULT_PLACE="head"
  else
    RESULT_STATE="clean"
    parent="$(git -C "${ROOT}" rev-parse --verify -q --short=12 HEAD~1 || true)"
    if [[ -n "${parent}" ]]; then
      RESULT_COMMIT="${parent}"
      RESULT_PLACE="parent"
    else
      RESULT_COMMIT="${RESULT_HEAD}"
      RESULT_PLACE="root"
    fi
  fi
  RESULT_MACHINE="$(machine_dir_name)"
  RESULTS_DIR="${ROOT}/bench-results/${RESULT_COMMIT}/${RESULT_MACHINE}"
}

prepare_results_dir() {
  select_results_dir
  mkdir -p "${RESULTS_DIR}"
}

explain_results_dir() {
  case "${RESULT_PLACE}" in
    head)
      echo "Modificări locale: rezultatele sunt pe commitul curent ${RESULT_COMMIT}."
      ;;
    parent)
      echo "Fără modificări locale: rezultatele sunt pe commitul anterior ${RESULT_COMMIT} (HEAD este ${RESULT_HEAD})."
      ;;
    root)
      echo "Primul commit nu are părinte: rezultatele sunt pe ${RESULT_COMMIT}."
      ;;
  esac
  echo "Director: ${RESULTS_DIR}"
}
