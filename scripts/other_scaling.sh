#!/bin/bash

cd ../

HOSTFILE=otherHostFile.txt

MaxClique() {
  for i in {1..5}; do
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel depthbounded -d 2 --hpx:threads 16  >> max_clique_scaling_depthbounded_$1.txt
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel budget -b 10000000 --hpx:threads 16 >> max_clique_scaling_budget_$1.txt
      timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/maxclique-16 -f test/brock800_1.clq --skel stacksteal --hpx:threads 16 >> max_clique_scaling_stacksteal_$1.txt
  done
}

NSHivert() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 51 --skel budget -b 10000000 --hpx:threads 16  >> ns_hivert_budget_$1.txt

		timeout 3600 mpiexec -n $1 -f $HOSTFILE ./build/install/bin/NS-hivert -g 51 --skel stacksteal --hpx:threads 16  >> ns_hivert_stack_steal_$1.txt
	done
}

SIP() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 ./build/install/bin/sip  --pattern test/meshes-CVIU11/patterns/pattern3 --target test/meshes-CVIU11/targets/target401 --skel depthbounded -d 8 --hpx:threads 16  >> sip_scaling_depthbounded_$1_2000.txt

		timeout 3600 mpiexec -n $1 ./build/install/bin/sip /sip  --pattern test/meshes-CVIU11/patterns/pattern3 --target test/meshes-CVIU11/targets/target401 --skel budget -b 100000 --hpx:threads 16 >> sip_scaling_budget_$1_2000.txt
		
		timeout 3600 mpiexec -n $1 ./build/install/bin/sip --pattern test/meshes-CVIU11/patterns/pattern3 --target test/meshes-CVIU11/targets/target401 --skel stacksteal --hpx:threads 16 >> sip_scaling_stacksteal_$1_2000.txt
	
	done

}

appendToHostFile() {
  for i in $(seq $1 $2); do
    echo 130.209.255.$i >> otherHostFile.txt
  done
}

echo '130.209.255.3' > otherHostFile.txt 
SIP 1
appendToHostFile 2 2
SIP 2

exit 0

