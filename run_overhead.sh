#!/bin/bash

createHostFile() {
	echo "130.209.255.4" > hostfile.txt
	for i in $(seq $1 $2); do
		echo "130.209.255.${i}" >> hostfile.txt
	done
}

for i in {1..5}; do
	mpiexec -n 1 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_4.clq --skel depthbounded -d 2 --metrics --hpx:threads 16 >> d_i_1_metrics_reg.txt 
done

for i in {1..5}; do
	mpiexec -n 1 -f hostfile.txt ./build/install/bin/NS-hivert -g 47 --skel budget -b 10000000 --metrics --hpx:threads 16 >> d_2_m_ns_ietrics_reg.txt 
done


createHostFile 5 5

for i in {1..5}; do
	mpiexec -n 2 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_4.clq --skel depthbounded -d 2 --metrics --hpx:threads 16 >> d_2_m_ietrics_reg.txt 
done

for i in {1..5}; do
	mpiexec -n 2 -f hostfile.txt ./build/install/bin/NS-hivert -g 47 --skel budget -b 10000000 --metrics --hpx:threads 16 >> d_2_m_ietrics_reg.txt 
done


createHostFile 5 7

for i in {1..5}; do
	mpiexec -n 4 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_4.clq --skel depthbounded -d 2 --metrics --hpx:threads 16 >> node_d_4__nmetrics_reg.txt 
done

for i in {1..5}; do
	mpiexec -n 4 ./build/install/bin/NS-hivert -g 47 --skel budget -b 10000000 --metrics --hpx:threads 16 >> ns_4__nreg_node.txt
done

createHostFile 5 11

for i in {1..5}; do
	mpiexec -n 8 -f hostfile.txt ./build/install/bin/maxclique-16 -f test/brock800_4.clq --skel depthbounded -d 2 --metrics --hpx:threads 16 >> d_8_metrics_reg_node.txt 
done

for i in {1..5}; do
	mpiexec -n 8 ./build/install/bin/NS-hivert -g 47 --skel budget -b 10000000 --metrics --hpx:threads 16 >> ns_8_reg_node.txt 
done

exit 0
