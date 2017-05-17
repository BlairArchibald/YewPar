# YewPar

A Collection of High Performance Parallel Skeletons for Tree Search Problems

**Warning: This library is currently experimental and should be considered very unstable**

Skeletons are designed to be used with
the [HPX](https://github.com/STEllAR-GROUP/hpx) parallel library/runtime

## Installation Instructions

YewPar and it's test applications can be installed
using [CMake](https://cmake.org/). For example:

```bash
git clone git@github.com:BlairArchibald/YewPar.git
cd YewPar
mkdir build
cd build

cmake \
 -DHPX_DIR=<pathToHPX>/hpx/hpx_build/lib/cmake/HPX/ \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_INSTALL_PREFIX=$(pwd)/install \
 ../
 
make && make install
```

Be sure to add `build/install/lib` and the hpx runtime libraries to your linker path.
