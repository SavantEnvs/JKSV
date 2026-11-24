#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer suite mayhem/build.sh built (build/mayhem-test/jksv_selftest,
# normal flags). JKSV ships no tests of its own (a Switch app), so mayhem/test/jksv_selftest.cpp pins
# the observable results of the code the fuzz targets reach: sanitized path names, UTF-16 dictionary
# words, URL/header handling, the WebDav PROPFIND listing, config load/save and backup-ZIP reading.
# Each check prints PASS/FAIL; a missing line (e.g. the program neutered to exit(0)) counts as failed.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

EXPECTED=33
runner="$SRC/build/mayhem-test/jksv_selftest"
if [ ! -x "$runner" ]; then
  echo "test.sh: $runner missing — mayhem/build.sh did not build the suite" >&2
  emit_ctrf jksv-selftest 0 "$EXPECTED"
  exit 1
fi

out="$("$runner" "$SRC/mayhem" 2>&1)"
echo "$out"
passed=$(grep -c '^PASS ' <<<"$out")
failed=$(grep -c '^FAIL ' <<<"$out")
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
emit_ctrf jksv-selftest "$passed" "$failed"
