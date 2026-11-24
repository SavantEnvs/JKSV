#!/usr/bin/env bash
#
# mayhem/build.sh — host (Linux) build of JKSV's untrusted-input parsers as libFuzzer targets.
#
# JKSV is a Nintendo Switch homebrew app (devkitPro/libnx Makefile, aarch64 Horizon target); it
# has no Linux build. The fuzzed JKSV sources (source/remote, source/config, source/fs/MiniUnzip,
# source/stringutil, source/keyboard, source/curl) are compiled UNMODIFIED for the host against
# mayhem/shim/: stand-ins for libnx (switch.h + its UTF helpers), FsLib (fslib.hpp over stdio),
# a silent logger, and a fake libcurl that serves the fuzz input as the HTTP response. json-c,
# tinyxml2, minizip and zlib are the real Debian libraries.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

JKSV_SOURCES=(
  source/stringutil.cpp
  source/keyboard/Dictionary.cpp
  source/remote/WebDav.cpp
  source/remote/GoogleDrive.cpp
  source/remote/Form.cpp
  source/remote/Storage.cpp
  source/remote/URL.cpp
  source/remote/Item.cpp
  source/curl/curl.cpp
  source/config/ConfigContext.cpp
  source/fs/MiniUnzip.cpp
)
SHIM_SOURCES=(mayhem/shim/support.cpp mayhem/shim/fake_curl.cpp mayhem/shim/strings_stub.cpp)
INCLUDES=(-I"$SRC/build/mayhem-gen" -I"$SRC/mayhem/shim" -I"$SRC/include" -I"$SRC/mayhem/harnesses")
CXXSTD=(-std=c++23 -include cstdint)
LIBS=(-ljson-c -ltinyxml2 -lminizip -lz)
HARNESSES=(fuzz_webdav fuzz_googledrive fuzz_config fuzz_miniunzip fuzz_sanitize_path fuzz_dictionary)

# Upstream declares ConfigContext::get_working_directory() `const` and defines it `const noexcept`;
# devkitPro's GCC accepts the mismatch, clang rejects it. Generate a copy of the upstream header with
# the declaration matching the definition (searched before include/), instead of editing upstream.
mkdir -p build/mayhem-gen/config
sed 's/fslib::Path get_working_directory() const;/fslib::Path get_working_directory() const noexcept;/' \
  include/config/ConfigContext.hpp > build/mayhem-gen/config/ConfigContext.hpp
grep -q 'get_working_directory() const noexcept;' build/mayhem-gen/config/ConfigContext.hpp

# build_objs <outdir> <cflags...>: compile JKSV + shim sources into <outdir>/ (gitignored build/).
build_objs() {
  local out="$1"; shift
  mkdir -p "$out"
  local s o pids=()
  for s in "${JKSV_SOURCES[@]}" "${SHIM_SOURCES[@]}"; do
    o="$out/$(echo "$s" | tr '/' '_').o"
    local extra=()
    # The fake libcurl calls JKSV's write callbacks through curl_write_callback, as libcurl does;
    # their real signatures take typed pointers, which -fsanitize=function flags at the call site.
    [ "$s" = mayhem/shim/fake_curl.cpp ] && extra=(-fno-sanitize=function)
    "$CXX" "${CXXSTD[@]}" "$@" "${extra[@]}" "${INCLUDES[@]}" -c "$s" -o "$o" &
    pids+=($!)
    if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  done
  for p in "${pids[@]}"; do wait "$p"; done
  "$CC" "$@" -c mayhem/shim/libnx_utf.c -o "$out/libnx_utf.o"
}

# 1) Sanitized, instrumented objects for the fuzz targets (the JKSV code itself carries ASan+UBSan).
FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1)
build_objs build/mayhem-fuzz "${FUZZ_FLAGS[@]}" -fsanitize=fuzzer-no-link
"$CXX" "${FUZZ_FLAGS[@]}" -c mayhem/lsan_off.cc -o build/mayhem-fuzz/lsan_off.o
"$CC" "${FUZZ_FLAGS[@]}" -c "$STANDALONE_FUZZ_MAIN" -o build/mayhem-fuzz/standalone_main.o
OBJS=(build/mayhem-fuzz/source_*.o build/mayhem-fuzz/mayhem_shim_*.o build/mayhem-fuzz/libnx_utf.o build/mayhem-fuzz/lsan_off.o)

# 2) Each harness twice: libFuzzer binary + run-once standalone reproducer.
for h in "${HARNESSES[@]}"; do
  "$CXX" "${CXXSTD[@]}" "${FUZZ_FLAGS[@]}" -fsanitize=fuzzer-no-link "${INCLUDES[@]}" \
    -c "mayhem/harnesses/$h.cpp" -o "build/mayhem-fuzz/h_$h.o"
  "$CXX" "${FUZZ_FLAGS[@]}" $LIB_FUZZING_ENGINE "build/mayhem-fuzz/h_$h.o" "${OBJS[@]}" "${LIBS[@]}" -o "$SRC/$h"
  "$CXX" "${FUZZ_FLAGS[@]}" "build/mayhem-fuzz/h_$h.o" build/mayhem-fuzz/standalone_main.o "${OBJS[@]}" "${LIBS[@]}" \
    -o "$SRC/$h-standalone"
done

# 3) Functional oracle: the same JKSV sources with normal flags, driven by known-answer tests.
build_objs build/mayhem-test -O2 $COVERAGE_FLAGS
"$CXX" "${CXXSTD[@]}" -O2 $COVERAGE_FLAGS "${INCLUDES[@]}" mayhem/test/jksv_selftest.cpp \
  build/mayhem-test/source_*.o build/mayhem-test/mayhem_shim_*.o build/mayhem-test/libnx_utf.o "${LIBS[@]}" \
  -o build/mayhem-test/jksv_selftest
