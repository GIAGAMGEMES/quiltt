{
  description = "quiltt";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          name = "quiltt";
          src = self;
          
          nativeBuildInputs = with pkgs; [
            pkg-config
            wrapGAppsHook
            lua
          ];
          
          buildInputs = with pkgs; [
            gtk3
            vte
            glib
            pango
            gdk-pixbuf
            lua
          ];
          
          buildPhase = ''
            $CC src/main.c -o quiltt `pkg-config --cflags --libs gtk+-3.0 vte-2.91` -llua
          '';
          
          installPhase = ''
            mkdir -p $out/bin
            cp quiltt $out/bin/
            
            mkdir -p $out/share/applications
            cp quiltt.desktop $out/share/applications/
          '';
        };
        
        devShell = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            pkg-config
            gcc
            gtk3
            vte
            glib
            pango
            gdk-pixbuf
            lua
          ];
        };
      }
    );
}
