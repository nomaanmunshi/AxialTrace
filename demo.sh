#!/bin/sh
set -eu
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
(cd build && ctest --output-on-failure)
mkdir -p build/traces
for scenario in forward reversal arrival limits jam frozen slow; do
    build/axialtrace "$scenario" > "build/traces/$scenario.csv"
    build/axialtrace verify "build/traces/$scenario.csv"
done
build/axialtrace verify tests/fixtures/correct_stop.csv
if build/axialtrace verify tests/fixtures/late_stop.csv; then
    echo "Verifier accepted the deliberately faulty trace" >&2
    exit 1
else
    status=$?
    if [ "$status" -ne 1 ]; then exit "$status"; fi
fi
