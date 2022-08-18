{
  description = "Image viewer for image processing experts";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = rec {
          vpv = pkgs.stdenv.mkDerivation rec {
            pname = "vpv";
            version = "0.7.1";
            src = ./.;

            cmakeFlags = [
              "-DUSE_GDAL=ON"
              "-DVPV_VERSION=${version}"
            ];
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.pkgconfig
            ];
            buildInputs = [
              pkgs.libpng
              pkgs.libtiff
              pkgs.libjpeg
              pkgs.SDL2
              pkgs.gdal

              # broken currently on nixpkgs (2021-08-18):
              #pkgs.octave
              #pkgs.octavePackages.image

              # see maybe https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/rust.section.md#compiling-non-rust-packages-that-include-rust-code-compiling-non-rust-packages-that-include-rust-code
              # pkgs.cargo
            ];
          };

          default = vpv;
        };
      }
    );
}
