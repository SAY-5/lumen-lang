#!/usr/bin/env bash
# Builds an instrumented binary, runs the test suite, and reports line coverage
# for src/ via gcovr. Enforces a floor read from tests/coverage_floor.txt unless
# --report-only is passed.
#
# The floor encodes the v1 coverage gate: it is set 5 percentage points above
# the pre-v1 baseline, so the gate fails if the comprehensive v1 suite (unit +
# property + corpus) ever regresses below that bar.
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build-cov}
REPORT_ONLY=0
[ "${1:-}" = "--report-only" ] && REPORT_ONLY=1

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DLUMEN_COVERAGE=ON \
  -DLUMEN_BUILD_BENCH=OFF -DCMAKE_CXX_COMPILER="${CXX:-g++}"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

# Pick a gcov matching the compiler so the .gcda version is understood.
GCOV_BIN=${GCOV:-gcov}
case "${CXX:-g++}" in
  *g++-12*) GCOV_BIN=gcov-12 ;;
  *g++-13*) GCOV_BIN=gcov-13 ;;
esac

# Measure only the interpreter sources under src/. Print the full summary for
# visibility, then extract the line-coverage percentage.
SUMMARY=$(gcovr --root . --gcov-executable "$GCOV_BIN" \
  --filter "$(pwd)/src/" --print-summary "$BUILD_DIR" 2>&1)
echo "$SUMMARY"
PCT=$(printf '%s\n' "$SUMMARY" | awk '/^lines:/ {gsub(/%/,"",$2); print $2; exit}')

if [ -z "$PCT" ]; then
  echo "coverage: failed to parse gcovr output" >&2
  exit 2
fi

echo "coverage: src/ line coverage = ${PCT}%"

if [ "$REPORT_ONLY" -eq 1 ]; then
  exit 0
fi

FLOOR=$(cat tests/coverage_floor.txt)
echo "coverage: floor = ${FLOOR}% (v1 gate = pre-v1 baseline + 5pp)"

awk -v p="$PCT" -v f="$FLOOR" 'BEGIN { exit (p + 0.0001 >= f) ? 0 : 1 }' || {
  echo "coverage: ${PCT}% is below the ${FLOOR}% floor" >&2
  exit 1
}
echo "coverage: gate passed"
