#!/bin/bash

if [ $# == 2 ];
  then
    N_JOBS=$1
else
  N_JOBS=4
fi

cmake -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
           -DHPX_DIR=$HOME/sandbox/YewParInstall/hpx-1.2.1/build/install/lib/cmake/HPX \
           -DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=16 \
           -DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220 \
           -DYEWPAR_BUILD_APPS_SIP_NWORDS=128 \
           ../
make -j${N_JOBS}
make install

