#!/bin/bash

cd ../

HPX_THREADS=--hpx:threads 8
SIP_R01_PATTERN=test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/pattern
SIP_R01_TARGET=test/newSIPbenchmarks/si/si2_rand_r01_600/si2_r01_m600.06/target

rmHostFile() {
	if [ -f hostfile.txt ]
	then
		rm hostfile.txt
	fi
}

runMaxCliqueDepthBoundedSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f $2 --skel depthbounded -d $3 $HPX_THREADS >> results/MaxClique_depthbounded_search_metrics_$1.txt
	done
}

runMaxCliqueBudgetSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f $2 --skel budget -b $3 $HPX_THREADS >> results/MaxClique_budget_search_metrics_$1.txt
	done
}

runMaxCliqueStackStealSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f $2 --skel stacksteal --chunked -d $3 $HPX_THREADS >> results/MaxClique_stacksteal_search_metrics_$1.txt
	done
}

runNSHivertBudgetSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g $2 --skel budget -b $3 $HPX_THREADS >> results/NS-hivert_budget_search_metrics_$1.txt
	done
}

runNSHivertStackStealSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/installl/bin/NS-hivert -g $2 --skel stacksteal --chunked $HPX_THREADS >> results/NS-hivert_stacksteal_search_metrics_$1.txt
	done
}

runNSHivertDepthBoundedSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g $2 --skel depthbounded -d $3 --chunked $HPX_THREADS >> results/NS-hivert_depthbounded_search_metrics_$1.txt
	done
}

runSIPDepthBoundedSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --pattern $2 --target $3 --skel depthbounded -d $4 $HPX_THREADS >> results/SIP_depthbounded_search_metrics_$1.txt
	done
}

runSIPBudgetSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --patern $2 --target $3 --skel budget -b $4 $HPX_THREADS >> results/SIP_budget_search_metrics_$1.txt
	done
}

runSIPStackStealSearches() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f $2 --skel stacksteal --chunked $HPX_THREADS >> resulsts/SIP_stacksteal_search_metrics_$1.txt
}

appendToHostFile() {
	for i in $(seq $1 $2); do
		echo 130.209.255.$i >> hostfile.txt
	done
}

rmHostFile

appendToHostFile 4 7
runSIPDepthBoundedSearches 50 $SIP_R01_PATTERN $SIP_R01_TARGET 4
appendToHostFile 8 11
runSIPDepthBoundedSearches 100 $SIP_R01_PATTERN $SIP_R01_TARGET 4
appendToHostFile 12 19
runSIPDepthBoundedSearches 150 $SIP_R01_PATTERN $SIP_R01_TARGET 4
runSIPDepthBoundedSearches 200 $SIP_R01_PATTERN $SIP_R01_TARGET 4
runSIPDepthBoundedSearches 250 $SIP_R01_PATTERN $SIP_R01_TARGET 4

rmHostFile

exit 0
