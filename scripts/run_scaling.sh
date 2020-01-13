#!/bin/bash

cd ../

HOSTFILE=hostfile.txt

MaxClique() {

  INSTANCE=$1
  NPROCESSES=$4

  for i in {1..5}; do
    for j in {1..3}; do
      mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f p_hat1500-$i.clq --skel depthbounded -d 2 >> max_clique_scaling_depthbounded.txt
    done
  done

  for i in {1..5}; do
    for j in {1..3}; do
      mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f p_hat1500-$i.clq --skel budget -b 1000000 >> max_clique_scaling_budget.txt
    done
  done

  for i in {1..5}; do
    for j in {1..3}; do
      mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/maxclique-16 -f p_hat1500-$i.clq --skel stacksteal --chunked >> max_clique_scaling_stacksteal.txt
    done
  done

}

NSHivert() {

  G=$1
  D=$2
  B=$3
  NPROCESSES=$4
  HOSTFILE=$5

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/NS-hivert -g $G --skel depthbounded -d $D >> ns-hivert_scaling_depthbounded.txt
  done

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/NS-hivert -g $G --skel budget -b $B >> ns-hivert_scaling_budget.txt
  done

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/NS-hivert -g $G --skel stacksteal --chunked >> ns-hivert_scaling_stacksteal.tzt
  done

}


Sip() {

  PATTERN=$1
  TARGET=$2
  D=$3
  B=$4
  NPROCESSES=$5

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/sip --pattern $PATTERN --target $TARGET --skel depthbounded -d $D >> sip_scaling_depthbounded.txt
  done

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/sip --pattern $PATTERN --target $TARGET --skel budget -b $B >> sip_scaling_budget.txt
  done

  for i in {1..5}; do
    mpiexec -n $NPROCESSES -f $HOSTFILE ./build/install/bin/sip --pattern $PATTERN --target $TARGET --skel stacksteal --chunked >> sip_scaling_stacksteal.txt
  done

}
