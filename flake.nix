{
  description = "A fast, image-capable terminal EPUB reader";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixpkgsDarwinIntel.url = "github:NixOS/nixpkgs/nixpkgs-26.05-darwin";
  };

  outputs =
    {
      nixpkgs,
      nixpkgsDarwinIntel,
      ...
    }:
    let
      systems = [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          packageSet =
            if system == "x86_64-darwin" then nixpkgsDarwinIntel else nixpkgs;
          pkgs = packageSet.legacyPackages.${system};
          epubworm = pkgs.callPackage ./nix/package.nix { };
        in
        {
          inherit epubworm;
          default = epubworm;
        }
      );
    };
}
