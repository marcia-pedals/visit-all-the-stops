{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.gcc
    pkgs.cmake
    pkgs.ninja
    pkgs.nodejs_20
    pkgs.nodePackages.vercel
  ];
}
