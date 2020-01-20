#!/bin/bash

cd ../

HOSTFILE=hostfile.txt

MaxClique() {
  NPROCESSES=$1
  for i in {1..5}; do
      timeout 3600 mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel depthbounded -d 2 --scaling >> max_clique_scaling_depthbounded_$1.txt

      timeout 3600 mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel budget -b 10000000 --scaling >> max_clique_scaling_budget_$1.txt

      timeout 3600 mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel stacksteal --scaling >> max_clique_scaling_stacksteal_$1.txt
  done
}

appendToHostFile() {
  for i in $(seq $1 $2); do
    echo 130.209.255.$i >> hostfile.txt
  done
}

echo '130.209.255.4' > hostfile.txt 
MaxClique 16
appendToHostFile 5 5
MaxClique 32
appendToHostFile 6 7
MaxClique 64
appendToHostFile 8 11
MaxClique 128
appendToHostFile 12 19
MaxClique 256
appendToHostFile 20 20
MaxClique 274

exit 0
