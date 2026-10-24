{
  description = "Linux software for SF100/SF600";

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ self.overlays.default ];
        };
      in {

        legacyPackages = pkgs;

        packages = {
          inherit (pkgs) sf100;
          default = pkgs.sf100;
        };

        formatter = pkgs.nixfmt;
      }) // {
        overlays.default = final: prev: {
          sf100 = final.callPackage ({ stdenv, gnumake, pkg-config, libusb }:
            stdenv.mkDerivation {
              pname = "sf100";
              version = "unstable";

              src = ./.;

              nativeBuildInputs = [ gnumake pkg-config ];
              buildInputs = [ libusb ];

              installPhase = ''
                install -Dv -m 0755 dpcmd $out/bin/dpcmd
                install -Dv -m 0644 ChipInfoDb.dedicfg $out/share/DediProg/ChipInfoDb.dedicfg
                install -Dv -m 0644 60-dediprog.rules $out/lib/udev/rules.d/60-dediprog.rules
              '';
            }) { };
        };
      };
}
