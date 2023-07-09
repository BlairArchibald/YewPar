with import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/23.05.tar.gz") {};
let
  boostpkg = boost179;
in
stdenv.mkDerivation rec {
  name = "YewPar";
  version = "0.0.2";

  buildInputs = [ boostpkg mpich hpx gperftools hwloc ];
  nativeBuildInputs = [ cmake ];

  src =
    let
      inDir = path : dir : pkgs.lib.hasPrefix (toString dir) (toString path);
      filter = path : type : baseNameOf path == "CMakeLists.txt"
        || inDir path ./apps
        || inDir path ./lib;
    in builtins.filterSource filter ./.;

  enableParallelBuilding = true;

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=14"
    "-DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220"
    "-DYEWPAR_BUILD_APPS_SIP_NWORDS=128"
  ];
}
