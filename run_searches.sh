#!/bin/bash

runEnumBudgetSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 48 --skel budget -b 10000000 >> results/NS-hivert_budget_search_metrics_$1.txt
	done
}

runEnumStackStealSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/installl/bin/NS-hivert -g 48 --skel stacksteal --chunked >> results/NS-hivert_stacksteal_search_metrics_$1.txt
	done
}

runEnumDepthBoundedSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 48 --skel depthbounded -d 35 --chunked >> results/NS-hivert_depthbounded_search_metrics_$1.txt
	done
}

rmHostFile() {
	if [ -f hostfile.txt ]
	then
		rm hostfile.txt
	fi
}

runSipDepthBoundedSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --pattern test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/pattern --target test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/target --skel budget -b 100000 --hpx:threads 8 >> results/SIP_budget_search_metrics_$1.txt
	done
}

runSipStackStealSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --pattern test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/pattern --target test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/target --skel stacksteal --chunked --hpx:threads 8 >> results/SIP_stacksteal_search_metrics_$1.txt
	done
}

appendToHostFile() {
	for i in $(seq $1 $2); do
		echo 130.209.255.$i >> hostfile.txt
	done
}

rmHostFile

appendToHostFile 4 5
appendToHostFile 6 7
runSipStackStealSearches 50
appendToHostFile 8 11
runSipStackStealSearches 100
appendToHostFile 12 19
runSipStackStealSearches 150
runSipStackStealSearches 200
runSipStackStealSearches 250

exit 0
