#!/usr/bin/python3
import subprocess
import re
import os

# Very basic test script. Mainly just to check the applications run for a small set of instances

# Maxclique Test
INSTALL_LOC = "../build"
MCBenchmarks=["data/brock200_1.clq", "data/brock200_2.clq", "data/brock200_3.clq", "data/brock200_4.clq"]

ldpath = os.environ['LD_LIBRARY_PATH']
MCBinary="LD_LIBRARY_PATH={}:{}/install/lib {}/install/bin/maxclique-8".format(ldpath, INSTALL_LOC, INSTALL_LOC)
NSBinary="LD_LIBRARY_PATH={}:{}/install/lib {}/install/bin/NS-hivert".format(ldpath, INSTALL_LOC, INSTALL_LOC)

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

def runNumericalSemigroups(skeleton, depth, additionalArgs = ""):
    cmd = "{} --skeleton-type {} -d {} --hpx:threads 1 {}"\
             .format(NSBinary, skeleton, depth, additionalArgs)
    try:
        res = subprocess.check_output(cmd, shell=True)
    except subprocess.CalledProcessError:
        FAILED.append(cmd)
        return

    # Just check it outputs something we expect (rather than a real result for now)
    found = False
    for line in res.decode("utf-8").splitlines():
        match = re.search("Results Table:", line)
        if match:
            found = True

    if not found:
        FAILED.append(cmd)

for inst in MCBenchmarks:
    runMaxClique("seq" , inst)
    runMaxClique("par" , inst, "--spawn-depth 1")
    runMaxClique("dist", inst, "--spawn-depth 1")
    runMaxClique("seq-decision" , inst, "--decisionBound 21")
    runMaxClique("par-decision" , inst, "--decisionBound 21 --spawn-depth 1")
    runMaxClique("dist-decision", inst, "--decisionBound 21 -- spawn-depth 1")
    runMaxClique("ordered", inst, "--spawn-depth 1")
    runMaxClique("dist-recompute", inst, "--spawn-depth 1")
    runMaxClique("indexed", inst)

runNumericalSemigroups("seq", 30)
runNumericalSemigroups("seq-stack", 30)
runNumericalSemigroups("dist", 30)
runNumericalSemigroups("indexed", 30)

# Output Results
if len(FAILED) > 0:
    print("FAILED {} TESTS: ".format(len(FAILED)))
    for f in FAILED:
        print(f)
else:
    print("ALL TESTS SUCCEEDED")
