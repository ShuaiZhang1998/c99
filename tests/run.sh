#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
CC="${BUILD_DIR}/c99cc"

echo "==> runner: ROOT=${ROOT_DIR}"
echo "==> runner: CC=${CC}"

cleanup() {
  rm -f "${ROOT_DIR}"/c99cc_stdio_tmp*.txt
}
trap cleanup EXIT

if [[ ! -x "${CC}" ]]; then
  echo "error: ${CC} not found or not executable"
  echo "hint: build first: cmake -S . -B build -DLLVM_DIR=\$(llvm-config --cmakedir) && cmake --build build -j"
  exit 1
fi

TMP_DIR="${BUILD_DIR}/test_tmp"
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"

pass=0
fail=0

run_ok() {
  local src="$1"
  local base
  base="$(basename "${src}" .c)"
  local exe="${TMP_DIR}/${base}.out"
  local outlog="${TMP_DIR}/${base}.out.log"
  local errlog="${TMP_DIR}/${base}.err.log"
  local args_line
  args_line="$(grep -Eo '^[[:space:]]*//[[:space:]]*ARGS:[[:space:]].+' "${src}" \
    | head -n1 | sed -E 's/.*ARGS:[[:space:]]*//' || true)"
  local expect
  expect="$(grep -Eo '^[[:space:]]*//[[:space:]]*EXPECT:[[:space:]]*-?[0-9]+' "${src}" | head -n1 | sed -E 's/.*EXPECT:[[:space:]]*//')"
  if [[ -z "${expect}" ]]; then
    echo "FAIL(ok): ${src}"
    echo "  missing // EXPECT: <int>"
    fail=$((fail+1))
    return
  fi

  set +e
  if [[ -n "${args_line}" ]]; then
    read -r -a extra_args <<< "${args_line}"
    "${CC}" "${src}" "${extra_args[@]}" -o "${exe}" >"${outlog}" 2>"${errlog}"
  else
    "${CC}" "${src}" -o "${exe}" >"${outlog}" 2>"${errlog}"
  fi
  local rc=$?
  set -e
  if [[ ${rc} -ne 0 ]]; then
    echo "FAIL(ok): ${src}"
    echo "  compiler exited ${rc}, expected success"
    echo "---- stderr ----"
    cat "${errlog}"
    fail=$((fail+1))
    return
  fi

  set +e
  "${exe}" >"${outlog}" 2>"${errlog}"
  local run_rc=$?
  set -e

  if [[ "${run_rc}" != "${expect}" ]]; then
    echo "FAIL(ok): ${src}"
    echo "  exit code ${run_rc}, expected ${expect}"
    fail=$((fail+1))
    return
  fi

  echo "PASS(ok): ${src}"
  pass=$((pass+1))
}

run_err() {
  local src="$1"
  local base
  base="$(basename "${src}" .c)"
  local exe="${TMP_DIR}/${base}.out"
  local outlog="${TMP_DIR}/${base}.out.log"
  local errlog="${TMP_DIR}/${base}.err.log"
  local args_line
  args_line="$(grep -Eo '^[[:space:]]*//[[:space:]]*ARGS:[[:space:]].+' "${src}" \
    | head -n1 | sed -E 's/.*ARGS:[[:space:]]*//' || true)"
  local needle
  needle="$(grep -Eo '^[[:space:]]*//[[:space:]]*ERROR:[[:space:]].+' "${src}" | head -n1 | sed -E 's/.*ERROR:[[:space:]]*//')"
  if [[ -z "${needle}" ]]; then
    echo "FAIL(err): ${src}"
    echo "  missing // ERROR: <substring>"
    fail=$((fail+1))
    return
  fi

  set +e
  if [[ -n "${args_line}" ]]; then
    read -r -a extra_args <<< "${args_line}"
    "${CC}" "${src}" "${extra_args[@]}" -o "${exe}" >"${outlog}" 2>"${errlog}"
  else
    "${CC}" "${src}" -o "${exe}" >"${outlog}" 2>"${errlog}"
  fi
  local rc=$?
  set -e

  if [[ ${rc} -eq 0 ]]; then
    echo "FAIL(err): ${src}"
    echo "  compiler succeeded, expected failure"
    fail=$((fail+1))
    return
  fi

  if ! grep -Fq "${needle}" "${errlog}"; then
    echo "FAIL(err): ${src}"
    echo "  stderr does not contain expected substring:"
    echo "  '${needle}'"
    echo "---- stderr ----"
    cat "${errlog}"
    fail=$((fail+1))
    return
  fi

  echo "PASS(err): ${src}"
  pass=$((pass+1))
}

echo "==> Running OK tests"
shopt -s nullglob
for t in "${ROOT_DIR}/tests/ok/"*.c; do
  run_ok "${t}"
done

echo "==> Running ERR tests"
for t in "${ROOT_DIR}/tests/err/"*.c; do
  run_err "${t}"
done

echo "==> Summary: PASS=${pass}, FAIL=${fail}"
if [[ ${fail} -ne 0 ]]; then
  exit 1
fi
