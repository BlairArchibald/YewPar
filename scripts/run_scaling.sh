#!/bin/bash

cd ../

HOSTFILE=hostfile.txt

MaxClique() {
  for i in {1..5}; do
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel depthbounded -d 2 --hpx:threads 16  >> max_clique_scaling_depthbounded_$1.txt
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel budget -b 10000000 --hpx:threads 16 >> max_clique_scaling_budget_$1.txt
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel stacksteal --hpx:threads 16 >> max_clique_scaling_stacksteal_$1.txt
  done
}

NSHivert() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 52 --skel budget -b 10000000 --hpx:threads 16  >> ns_hivert_budget_$1_52.txt

		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 52 --skel stacksteal --hpx:threads 16  >> ns_hivert_stack_steal_$1_52.txt
	done
}

appendToHostFile() {
  for i in $(seq $1 $2); do
    echo 130.209.255.$i >> hostfile.txt
  done
}

echo '130.209.255.4' > hostfile.txt 
NSHivert 1
appendToHostFile 5 5
NSHivert 2
appendToHostFile 6 7
NSHivert 4
appendToHostFile 8 11
NSHivert 8
appendToHostFile 12 19
NSHivert 16
appendToHostFile 20 20
NSHivert 17

exit 0

