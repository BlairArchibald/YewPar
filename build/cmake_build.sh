#!/bin/bash

$HOME/sandbox/YewParInstall/cmake-3.14.4-Linux-x86_64/bin/cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
      -DHPX_DIR=$HOME/sandbox/YewParInstall/hpx-1.2.1/build/install/lib/cmake/HPX \
      -DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=16 \
      -DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220 \
      -DYEWPAR_BUILD_APPS_SIP_NWORDS=128 \
      ../

