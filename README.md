# YewPar

A Collection of High Performance Parallel Skeletons for Tree Search Problems

**Warning: This library is currently experimental and should be considered very unstable**

Skeletons are designed to be used with
the [HPX](https://github.com/STEllAR-GROUP/hpx) parallel library/runtime

## Installation Instructions

### Via Nix

The quickest way to get up and running is to use [Nix](https://nixos.org/nix/)
and the included [default.nix](default.nix) file to build YewPar (including the required
dependencies). Once you have Nix installed run:

```bash
nix-build
```

To build YewPar and it's test applications into `./result/`. 
Note: The first time you run this command Nix needs to build HPX which may take some time.

When executing the test applications first start a `nix-shell` to ensure the correct runtime libraries are loaded.

### Via CMake

YewPar and it's test applications can also be built using
[CMake](https://cmake.org/). For example:

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

## Available Skeletons

YewPar currently supports three types of search:

1. ~~Divide and Conquer~~ (currently unmaintained)
2. Tree Enumeration - Count the number of nodes in a tree
3. Decision Branch and Bound - Does a solution with bound *X* exist?
4. Optimisation Branch and Bound - Find a solution maximising an objective function

There are many skeletons available. An incomplete list is:

1. Sequential (Stack or Recursive) - No parallelism constructs added.
2. Parallel   - Using the hpx::async mechanism (similar to how we might use something like Cilk)
3. Dist       - Multi-locality support leveraging distributed workqueues
4. Indexed    - Track the path's through the tree and use this to *recompute* initial nodes rather than sending them
5. GenNode    - Steal directly from the stacks of other threads

Special Skeletons:

1. Ordered Skeleton for Branch and Bound Search - From [Replicable Parallel
   Branch and Bound
   Search](http://www.sciencedirect.com/science/article/pii/S0743731517302861)
   with a slightly different discrepancy order (count discrepancies, no
   accounting for the depth they occur at)

## Sample Applications

YewPar currently comes with a couple of example applications that are built
during the install. By default these binaries are placed in `${CMAKE_INSTALL_PREFIX}/bin` and require `${CMAKE_ISNTALL_PREFIX}/lib` to be on the linker path. Current applications are:

- ~~Divide and Conquer~~
  - ~~Fibonnaci~~

- Enumeration (Counting)
  - [Unbalanced Tree Search](https://sourceforge.net/p/uts-benchmark/wiki/Home/)
  - [Numerical Semigroups](https://arxiv.org/abs/1305.3831)
  - Fibonacci (for test purposes only)
  
- Decision:
  - k-clique (part of the maxcliuqe application)

- Branch and Bound
  - Maximum Clique
  - 0/1 Knapsack
  - Maximum Common Subgraph (via Clique encoding i.e very like Maximum Clique)

For a description of how to run the application you can pass the `-h` flag to the binary. A sample command line looks like follows:

```bash
mpiexec -n 2 ./install/bin/maxclique-8 --input-file brock200_1.clq --skeleton-type dist --spawn-depth 2 --hpx:threads 8
```
