#!/usr/bin/env bash
set -euo pipefail

echo "=== Rebuilding vertexcover with performance counter output ==="
cd /cluster/YewParVCTest/build
make vertexcover-16 -j2

echo ""
echo "=== Killing any running experiments ==="
pkill -9 vertexcover-16 || true
pkill -9 mpirun || true

echo ""
echo "=== Cleaning old results ==="
cd /cluster/YewParVCTest
rm -rf results_hpx_counters/logs/*
rm -f results_hpx_counters/hpx_counter_results.csv

echo ""
echo "=== Starting HPX counter experiment ==="
./hpx_counter_experiment.sh
