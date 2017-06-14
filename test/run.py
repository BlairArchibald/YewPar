#!/usr/bin/python3
import subprocess
import re
import os

# Very basic test script. Mainly just to check the applications run for a small set of instances

# Maxclique Test
INSTALL_LOC = "../build"
MCBenchmarks=["data/brock200_1.clq", "data/brock200_2.clq", "data/brock200_3.clq", "data/brock200_4.clq"]

ldpath = os.environ['LD_LIBRARY_PATH']
MCBinary="LD_LIBRARY_PATH={}:{}/install/lib {}/install/bin/maxclique-4".format(ldpath, INSTALL_LOC, INSTALL_LOC)

FAILED = []

def runMaxClique(skeleton, instance, additionalArgs = ""):
    cmd = "{} --skeleton-type {} --input-file {} --hpx:threads 1 {}"\
             .format(MCBinary, skeleton, instance, additionalArgs)
    try:
        res = subprocess.check_output(cmd, shell=True)
    except subprocess.CalledProcessError:
        FAILED.append(cmd)
        return

    # Just check it outputs something we expect (rather than a real result for now)
    found = False
    for line in res.decode("utf-8").splitlines():
        match = re.search("cpu", line)
        if match:
            found = True

    if not found:
        FAILED.append(cmd)

for inst in MCBenchmarks:
    runMaxClique("seq" , inst)
    runMaxClique("par" , inst)
    runMaxClique("dist", inst)
    runMaxClique("seq-decision" , inst, "--decisionBound 21")
    runMaxClique("par-decision" , inst, "--decisionBound 21")
    runMaxClique("dist-decision", inst, "--decisionBound 21")
    runMaxClique("ordered", inst, "--spawn-depth 1")

# Output Results
if len(FAILED) > 0:
    print("FAILED {} TESTS: ".format(len(FAILED)))
    for f in FAILED:
        print(f)
else:
    print("ALL TESTS SUCCEEDED")
