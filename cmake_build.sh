#!/bin/bash

cd build

cmake -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
           -DHPX_DIR=$HOME/ruairidh/sandbox/hpx-1.2.1/build/install/lib/cmake/HPX \
           -DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=16 \
           -DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220 \
           -DYEWPAR_BUILD_APPS_SIP_NWORDS=128 \
           ../

exit 0

