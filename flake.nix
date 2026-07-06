{
  description = "make2flake build environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "make2flake";
        version = "0.1.0";

        src = ./.;

        buildInputs = [ ]; 
        
        buildPhase = ''
          make
        '';

        installPhase = ''
          install -Dm755 bin/make2flake $out/bin/make2flake
        '';
      };

      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          gnumake
        ];
      };
    };
}
