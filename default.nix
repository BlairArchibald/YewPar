with import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/78a17f5765f24ae24ee87224dad16862451295d2.tar.gz") {};
let
stdenv = overrideCC pkgs.stdenv pkgs.gcc7;
boost = boost164;

newHPX = stdenv.mkDerivation rec {
  name = "hpx-new";
  src = fetchFromGitHub {
    owner = "STEllAR-GROUP";
    repo = "hpx";
    rev = "51d3d007e5e7d1bad5eccd587569cc267b061246";
    sha256 = "1bydsxsblgrqykw4rw50qmlbqn7q4msirfbcw3y9dv631svw34k5";
  };

  buildInputs = [ boost mpich2 hwloc gperftools];
  nativeBuildInputs = [ cmake pkgconfig python ];

  cmakeFlags = [
    "-DHPX_WITH_PARCELPORT_MPI=ON"
    "-DHPX_WITH_EXAMPLES=OFF"
    "-DHPX_WITH_TESTS=OFF"
    "-DMPI_LIBRARY=${mpich2}/lib/libmpi.so"
  ];

  enableParallelBuilding = true;

  meta = {
    description = "C++ standard library for concurrency and parallelism";
    homepage = https://github.com/STEllAR-GROUP/hpx;
    license = stdenv.lib.licenses.boost;
    platforms = [ "x86_64-linux" ]; # stdenv.lib.platforms.linux;
  };
};
in
stdenv.mkDerivation rec {
  name = "YewPar";
  version = "0.0.1";

  buildInputs = [ boost newHPX mpich2 gperftools hwloc ];
  nativeBuildInputs = [ cmake ];

  src =
    let
      inDir = path : dir : stdenv.lib.hasPrefix (toString dir) (toString path);
      filter = path : type : baseNameOf path == "CMakeLists.txt"
        || inDir path ./apps
        || inDir path ./lib;
    in builtins.filterSource filter ./.;

  enableParallelBuilding = true;
}
