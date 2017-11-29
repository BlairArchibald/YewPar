with import <nixpkgs> {};

let newHPX = pkgs.hpx.overrideDerivation (oldAttrs: {
  name = "hpx-new";
  src = fetchFromGitHub {
    owner = "STEllAR-GROUP";
    repo = "hpx";
    rev = "51d3d007e5e7d1bad5eccd587569cc267b061246";
    sha256 = "1bydsxsblgrqykw4rw50qmlbqn7q4msirfbcw3y9dv631svw34k5";
  };

  buildInputs = oldAttrs.buildInputs ++ [ openmpi gcc7 ];

  cmakeFlags = [
    "-DHPX_WITH_PARCELPORT_MPI=ON"
    "-DHPX_WITH_EXAMPLES=OFF"
    "-DHPX_WITH_TESTS=OFF"
  ];

});
in
stdenv.mkDerivation rec {
  name = "YewPar";
  version = "0.0.1";

  buildInputs = [
    gcc7
    boost
    newHPX
    openmpi
  ];

  src = ./.;

  nativeBuildInputs = [ cmake ];

  enableParallelBuilding = true;
}
