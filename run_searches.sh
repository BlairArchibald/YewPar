#!/bin/bash

for i in {1..5}; do
	mpiexec -n 250 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_4.clq --skel stacksteal --hpx:threads 4 >> stacksteal.txt
done


exit 0
