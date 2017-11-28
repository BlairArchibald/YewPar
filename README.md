# YewPar

A Collection of High Performance Parallel Skeletons for Tree Search Problems

**Warning: This library is currently experimental and should be considered very unstable**

Skeletons are designed to be used with
the [HPX](https://github.com/STEllAR-GROUP/hpx) parallel library/runtime

## Installation Instructions

### Via Nix

The quickest way to get up and running is to use [Nix](https://nixos.org/nix/)
and the included [default.nix] file to build YewPar (including the required
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

Many of the skeletons come in three variants:

1. Sequential - No parallelism constructs added.
2. Parallel   - Using the hpx::async mechanism (similar to how we might use something like Cilk)
3. Dist       - Multi-locality support leveraging distributed workqueues

We support three types of search:

1. Divide and Conquer
2. Decision Branch and Bound - Does a solution with bound *X* exist?
3. Optimisation Branch and Bound - Find a solution maximising an objective function

Special Skeletons:

1. Ordered Skeleton for Branch and Bound Search - From
   https://arxiv.org/abs/1703.05647 with a slightly different discrepancy order
   (count discrepancies, no accounting for the depth they occur at)

## Sample Applications

YewPar currently comes with a couple of example applications that are built
during the install. By default these binaries are placed in `${CMAKE_INSTALL_PREFIX}/bin` and require `${CMAKE_ISNTALL_PREFIX}/lib` to be on the linker path. Current applications are:

- Divide and Conquer
  - Fibonnaci

- Branch and Bound
  - Maximum Clique
  - Knapsack

For a description of how to run the application you can pass the `-h` flag to the binary. A sample command line looks like follows:

```bash
mpiexec -n 2 ./install/bin/maxclique-4 --input-file brock200_1.clq --skeleton-type dist --spawn-depth 2 --hpx:threads 8 --hpx:ini=hpx.stacks.small_size=0x20000
```

For expected deep trees it is likely you will need to increase the stack size as above.
