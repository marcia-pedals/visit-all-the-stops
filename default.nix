{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.gcc
    pkgs.cmake
    pkgs.ninja
    pkgs.boost.dev
  ];
  shellHook = ''
    export BOOST_ROOT=${pkgs.boost.dev}
  '';
}
