with import <nixpkgs> {};
let
  new_hpx = hpx.overrideAttrs (old : rec {
    buildInputs = old.buildInputs ++ [ mpich2 ];
    cmakeFlags = [
      "-DHPX_WITH_PARCELPORT_MPI=ON"
      "-DHPX_WITH_EXAMPLES=OFF"
      "-DHPX_WITH_TESTS=OFF"
      "-DMPI_LIBRARY=${mpich2}/lib/libmpi.so"
    ];
  });
in
stdenv.mkDerivation rec {
  name = "YewPar";
  version = "0.0.1";

  buildInputs = [ boost new_hpx gperftools mpich2 hwloc ];
  nativeBuildInputs = [ cmake ];

  src =
    let
      inDir = path : dir : stdenv.lib.hasPrefix (toString dir) (toString path);
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
