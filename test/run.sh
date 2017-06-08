#!/bin/bash

# Very basic test script. Mainly just to check the applications still run for a small set of instances

INSTALL_LOC=../build

# Maxclique Test
MCBenchmark=("data/brock200_1.clq" "data/brock200_2.clq" "data/brock200_3.clq" "data/brock200_4.clq")
MCBinary="LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${INSTALL_LOC}/install/lib ${INSTALL_LOC}/install/bin/maxclique-4"

echo "Running Tests"

FAILED=0
FAILED_CMDS=()

for b in ${MCBenchmark[*]}; do
    cmd="${MCBinary} --skeleton-type seq --input-file ${b} --hpx:threads 1"
    eval ${cmd} &>/dev/null
    if [[ $? != 0 ]]; then
       FAILED=$((FAILED + 1))
       FAILED_CMDS+=("${cmd}")
    fi
done

for b in ${MCBenchmark[*]}; do
    cmd="${MCBinary} --skeleton-type par --input-file ${b} --hpx:threads 1"
    eval ${cmd} &>/dev/null
    if [[ $? != 0 ]]; then
       FAILED=$((FAILED + 1))
       FAILED_CMDS+=("${cmd}")
    fi
done

for b in ${MCBenchmark[*]}; do
    cmd="${MCBinary} --skeleton-type dist --input-file ${b} --hpx:threads 1"
    eval ${cmd} &>/dev/null
    if [[ $? != 0 ]]; then
       FAILED=$((FAILED + 1))
       FAILED_CMDS+=("${cmd}")
    fi
done

if [[ ${FAILED} -gt 0 ]]; then
    echo "${FAILED} TESTS FAILED"
fi

for f in ${FAILED_CMDS[*]}; do
    echo "Failed: ${f}"
done
