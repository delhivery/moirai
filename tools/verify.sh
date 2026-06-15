#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"
VERIFY_COMPILER="${VERIFY_COMPILER:-gcc}"

cd "${ROOT}"

verify_one() {
  local compiler="$1"
  local cxx
  local stdlib
  local default_build_dir

  case "${compiler}" in
    gcc)
      cxx="${CXX_GCC:-g++}"
      stdlib="${CXX_STANDARD_LIBRARY_GCC:-libstdc++}"
      default_build_dir="${ROOT}/build-gcc"
      ;;
    clang)
      cxx="${CXX_CLANG:-clang++}"
      stdlib="${CXX_STANDARD_LIBRARY_CLANG:-libstdc++}"
      default_build_dir="${ROOT}/build-clang"
      ;;
    *)
      echo "Unknown VERIFY_COMPILER entry: ${compiler}" >&2
      exit 1
      ;;
  esac

  local build_dir="${BUILD_DIR:-${default_build_dir}}"
  local verify_log="${build_dir}.verify.log"

  rm -rf "${build_dir}"
  rm -f "${verify_log}"
  cmake -S "${ROOT}" -B "${build_dir}" -G Ninja \
    -DCMAKE_CXX_COMPILER="${cxx}" \
    -DCMAKE_CXX_STANDARD_LIBRARY="${stdlib}" \
    -Wno-dev 2>&1 | tee -a "${verify_log}"
  cmake --build "${build_dir}" -j "${BUILD_JOBS}" 2>&1 | tee -a "${verify_log}"
  ctest --test-dir "${build_dir}" --output-on-failure 2>&1 | tee -a "${verify_log}"
  if rg -n "warning:" "${verify_log}"; then
    echo "${compiler} build emitted warnings" >&2
    exit 1
  fi
}

case "${VERIFY_COMPILER}" in
  gcc|clang)
    verify_one "${VERIFY_COMPILER}"
    ;;
  both)
    verify_one gcc
    verify_one clang
    ;;
  *)
    echo "VERIFY_COMPILER must be gcc, clang, or both" >&2
    exit 1
    ;;
esac

git diff --check

STALE_PATTERN='Boost::graph|boost::|Node<Graph>|Edge<Graph>|Graph::null_vertex|graph_helpers|server_local|cluster_config|enumerate|recorder'
if rg -n "${STALE_PATTERN}" CMakeLists.txt modules src tests benchmarks include docs \
  --glob '!include/nlohmann/json.hpp' \
  --glob '!include/cameron314/**'; then
  echo "stale graph/header-era reference found" >&2
  exit 1
fi
