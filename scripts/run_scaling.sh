#!/bin/bash

cd ../

HOSTFILE=hostfile.txt

MaxClique() {
  for i in {1..5}; do
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel depthbounded -d 2 --scaling >> max_clique_scaling_depthbounded_$1.txt

      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel budget -b 10000000 --scaling >> max_clique_scaling_budget_$1.txt

      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel stacksteal --scaling >> max_clique_scaling_stacksteal_$1.txt
  done
}

NSHivert() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 51 --skel budget -b 10000000 >> ns_hivert_budget_$1.txt
		sleep 60
		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 51 --skel stacksteal >> ns_hivert_stack_steal_$1.txt
	done
}

appendToHostFile() {
  for i in $(seq $1 $2); do
    echo 130.209.255.$i >> hostfile.txt
  done
}

echo '130.209.255.4' > hostfile.txt 
NSHivert 16
appendToHostFile 5 5
NSHivert 32
appendToHostFile 6 7
NSHivert 64
appendToHostFile 8 11
NSHivert 128
appendToHostFile 12 19
NSHivert 256
appendToHostFile 20 20
NSHivert 274

exit 0
