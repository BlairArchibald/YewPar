#!/bin/bash --login

# PBS job options (name, compute nodes, job time)
#PBS -N YewPar_Scaling_NS
# Select 2 full nodes
#PBS -l select=2:ncpus=32
# Parallel jobs should always specify exclusive node access
#PBS -l place=scatter:excl
#PBS -l walltime=1:0:0

# Replace [budget code] below with your project code (e.g. t01)
#PBS -A sc038

# Change to the directory that the job was submitted from
cd $PBS_O_WORKDIR

# Load any required modules
source YewPar_env.sh

# Launch the parallel job
mpiexec_mpt -ppn 1 -n 2 ./build/install/bin/NS-hivert -g 52 --skel budget -b 10000000 --hpx:threads 64 > ns_cirrus_2.txt 2> my_stderr.txt
