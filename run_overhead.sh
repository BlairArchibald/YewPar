#!/bin/bash

createHostFile() {
	echo "130.209.255.4" > hostfile.txt
	for i in $(seq $1 $2); do
		echo "130.209.255.${i}" >> hostfile.txt
	done
}

runExpr() {
	for i in {1..5}; do
		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock400_1.clq --skel depthbounded -d 2 --$2 --hpx:threads 16 >> Results/Depthbounded/b400_$2_cost_$1.txt 

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock400_4.clq --skel depthbounded -d 2 --$2 --hpx:threads 16 >> Results/Depthbounded/b400_$2_cost_$1.txt 

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/p_hat700-3.clq --skel depthbounded -d 2 --$2 --hpx:threads 16 >> Results/Depthbounded/b400_$2_cost_$1.txt 

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock400_2.clq --skel depthbounded -d 2 --$2 --hpx:threads 16 >> Results/Depthbounded/b400_$2_cost_$1.txt 

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 46 --skel budget -b 10000000 --hpx:threads 16 --$2 >> Results/Budget/ns_46_$2_cost_$1.txt	

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 47 --skel budget -b 10000000 --hpx:threads 16 --$2 >> Results/Budget/ns_47_$2_cost_$1.txt

		timeout 3600 mpiexec -n $1 -f hostfile.txt ./build/install/bin/NS-hivert -g 45 --skel budget -b 10000000 --hpx:threads 16 --$2 >> Results/Budget/ns_45_$2_cost_$1.txt

		timeout 2700 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --pattern test/g34 --target test/g79 --skel stacksteal --hpx:threads 16 >> Results/StackSteal/sip_g34_g79_$2_cost_$1.txt

		timeout 2700 mpiexec -n $1 -f hostfile.txt ./build/install/bin/sip --pattern test/g35 --target test/g76 --skel stacksteal --hpx:threads 16 >> Results/StackSteal/sip_g35_g76_cost_$1.txt
	done
}

echo "130.209.255.4" > hostfile.txt
METRICS="nodes regularity backtracks"
for i in $METRICS; do
	runExpr 1 $i
	createHostFile 5 5
	runExpr 2 $i
	createHostFile 5 7
	runExpr 4 $i
	createHostFile 5 11
	runExpr 8 $i
done

exit 0
