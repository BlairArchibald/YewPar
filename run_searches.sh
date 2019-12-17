#!/bin/bash

runEnumBudgetSearches() {
	for i in {1..2}; do
		mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 48 --skel budget -b 10000000 >> results/NS-hivert_budget_search_metrics_$1.txt
	done
}

runEnumStackStealSearches() {
	for i in {1..5}; do
		mpiexec -n $1 -f hostfile.txt ./build/installl/bin/NS-hivert -g 48 --skel stacksteal --chunked >> results/NS-hivert_stacksteal_search_metrics_$1.txt
	done
}

runEnumDepthBoundedSearches() {
	for i in {1..5}; do
		mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 48 --skel depthbounded -d 35 --chunked >> results/NS-hivert_depthbounded_search_metrics_$1.txt
	done
}

rmHostFile() {
	if [ -f hostfile.txt ]
	then
		rm hostfile.txt
	fi
}

appendToHostFile() {
	for i in $(seq $1 $2); do
		echo 130.209.255.$i >> hostfile.txt
	done
}


rmHostFile

appendToHostFile 4 5
#runEnumBudgetSearches 32
appendToHostFile 6 7
#runEnumBudgetSearches 64
appendToHostFile 8 11
#runEnumBudgetSearches 128
appendToHostFile 12 19
runEnumBudgetSearches 250

rmHostFile

appendToHostFile 4 5
runEnumStackStealSearches 32
appendToHostFile 6 7
runEnumStackStealSearches 64
appendToHostFile 8 11
runEnumStackStealSearches 128
appendToHostFile 12 19
runEnumStackStealSearches 250

rmHostFile

appendToHostFile 4 5
runEnumDepthBoundedSearches 32
appendToHostFile 6 7
runEnumDepthBoundedSearches 64
appendToHostFile 8 11
runEnumDepthBoundedSearches 128
appendToHostFile 12 19
runEnumDepthBoundedSearches 250 

exit 0
