#!/bin/bash

<<<<<<< HEAD
INSTALL_LOC=$HOME/sandbox/YewParInstall/
cd build
$HOME/sandbox/YewParInstall/cmake-3.14.4-Linux-x86_64/bin/cmake -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
           -DHPX_DIR=${INSTALL_LOC}/hpx-1.2.1/build/install/lib/cmake/HPX \
=======
if [ $# == 2 ];
  then
    N_JOBS=$1
else
  N_JOBS=4
fi

cd = build
cmake -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
           -DHPX_DIR=$HOME/ruairidh/sandbox/hpx-1.2.1/build/install/lib/cmake/HPX \
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
           -DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=16 \
           -DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220 \
           -DYEWPAR_BUILD_APPS_SIP_NWORDS=128 \
           ../
<<<<<<< HEAD

exit 0
=======
make -j${N_JOBS}
make install
>>>>>>> cfcd49cc6bf7448ac95ff2a7bcee1d82dc8af8ce
