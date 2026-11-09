#!/bin/sh
set -eu
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
(cd build && ctest --output-on-failure)
mkdir -p build/traces
for scenario in forward reversal arrival limits jam frozen; do
    build/axialtrace "$scenario" > "build/traces/$scenario.csv"
done
