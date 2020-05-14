{
  description = "A flake for the construct matrix homeserver";

  outputs = { self, nixpkgs }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux"  "i686-linux"  "aarch64-linux" "x86_64-darwin" ];
  in {

    overlay = final: prev: rec {
      matrix-construct-source = let
        inherit (prev) lib linkFarm;
        srcFilter = n: t: (lib.hasSuffix ".cc" n || lib.hasSuffix ".h" n || lib.hasSuffix ".S" n
        || lib.hasSuffix ".md" n || t == "directory");
        repo = lib.cleanSourceWith { filter = srcFilter; src = lib.cleanSource ./.; };

        buildFileWith = root: name: type: rec {
          inherit name; file = "${root}/${name}";
          path = if type == "directory" then buildFarmFrom name file else "${file}";
        };
        buildFarm = root: lib.mapAttrsToList (buildFileWith root) (builtins.readDir root);
        buildFarmFrom = basename: root: linkFarm (lib.strings.sanitizeDerivationName basename) (buildFarm root);
      in buildFarmFrom "construct" self;

      matrix-construct = prev.callPackage ./nix/package {
        rev = if self ? rev then self.rev else "development";
        source = matrix-construct-source;
      };
    };

    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in self.overlay pkgs pkgs);

    defaultPackage = forAllSystems (system: self.packages.${system}.matrix-construct);

    nixosModules = {
      matrix-construct = import ./nix/module self;
    };
  };
}
