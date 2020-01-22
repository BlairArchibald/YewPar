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

runMaxClique() {
	
	for i in {1..5}; do
		timeout 4000 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel depthbounded -d 2 --hpx:threads 8 >> MaxClique_depthbounded_search_metrics_$1.txt
	done

	for i in {1..5}; do
		timeout 4000 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel budget -b 10000000 --hpx:threads 8 >> MaxClique_budget_search_metrics_$1.txt
	done
	
	for i in {1..5}; do
		timeout 4000 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel stacksteal --chunked --hpx:threads 8 >> MaxClique_depthbounded_search_metrics_$1.txt
	done

}

rmHostFile

appendToHostFile 4 11
runMaxClique 100
appendToHostFile 12 19
runMaxClique 150
runMaxClique 200
runMaxClique 250



exit 0
