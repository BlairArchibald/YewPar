#!/bin/bash

runSearches() {
	for i in {1..5}; do
		mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 48 --skel budget -b 10000000 >> results/NS-hivert_budget_search_metrics_$1.txt
	done
}

appendToHostFile() {
	for i in $(seq $1 $2); do
		echo '130.209.255.${i}' >> hostfile.txt
	done
}

if [ -f hostfile.txt ]
then
	rm hostfile.txt
done

appendToHostFile 4 5
runSearches 32
appendToHostFile 6 7
runSearches 64
appendToHostFile 8 11
runSearches 128
appendToHostFile 12 19
runSearches 250

exit 0
