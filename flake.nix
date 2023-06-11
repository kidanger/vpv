{
  description = "Image viewer for image processing experts";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        rustPlatform = pkgs.rustPlatform;
      in {
        packages = rec {
          vpv = pkgs.stdenv.mkDerivation rec {
            pname = "vpv";
            version = "0.8.2";
            src = ./.;

            cargoRoot = "src/fuzzy-finder";
            cargoDeps = rustPlatform.importCargoLock {
              lockFile = ./src/fuzzy-finder/Cargo.lock;
            };

            cmakeFlags = [
              "-DUSE_GDAL=ON"
              "-DUSE_OCTAVE=ON"
              "-DVPV_VERSION=${version}"
            ];

            nativeBuildInputs = with pkgs; [
              cmake
              pkgconfig
              rustPlatform.cargoSetupHook
              cargo
            ];

            buildInputs = with pkgs; [
              libpng
              libtiff
              libjpeg
              SDL2
              gdal

              octave
              # (2022-11-18) broken https://github.com/NixOS/nixpkgs/issues/186928
              #pkgs.octavePackages.image
            ];
          };

          default = vpv;
        };
      }
    );
}
