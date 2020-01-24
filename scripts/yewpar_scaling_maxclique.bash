#!/bin/bash --login

#PBS -N max_clique_2_loc
#PBS -l select=2:ncpus=36
#PBS -l place=scatter:excl
#PBS -l walltime=03:00:00

#PBS -A sc038

cd $PBS_O_WORKDIR

source YewPar_env.sh


for i in {1..5}; do
  mpiexec_mpt  ./build/install/bin/maxclique-16 -f test/p_hat1500-1.clq --skel depthbounded -d 2 --scaling >> max_clique_scaling_results_depthbounded.txt
  mpiexec_mpt  ./build/install/bin/maxclique-16 -f test/p_hat1500-1.clq --skel budget -b 10000000 --scaling >> max_clique_scaling_results_budget.txt
  mpiexec_mpt  ./build/install/bin/maxclique-16 -f test/p_hat1500-1.clq --skel stacksteal --chunked --scaling >> max_clique_scaling_results_stacksteal.txt
done

exit 0
