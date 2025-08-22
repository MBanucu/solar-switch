# shell.nix
{
}:
let
  development = true;
  pkgs = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/master.tar.gz";
  }) { config.allowUnfree = true; };

  masterOverlays = [
    (
      self: super:
      let
        nix-vscode-extensions = import (
          fetchTarball "https://github.com/nix-community/nix-vscode-extensions/archive/master.tar.gz"
        );
        nixpkgsMaster =
          import
            (builtins.fetchTarball {
              url = "https://github.com/NixOS/nixpkgs/archive/master.tar.gz";
            })
            {
              config.allowUnfree = true;
              overlays = [
                nix-vscode-extensions.overlays.default
              ];
            };
      in
      {
        vscode = (
          nixpkgsMaster.vscode-with-extensions.override {
            vscode = nixpkgsMaster.vscode;
            vscodeExtensions = with nixpkgsMaster.vscode-marketplace; [
              platformio.platformio-ide
              jnoortheen.nix-ide
              ms-vscode.cpptools
            ];
          }
        );
      }
    )
  ];
  pkgsMaster = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/master.tar.gz";
  }) { overlays = masterOverlays; config.allowUnfree = true; };

  myPython = pkgs.python3.withPackages (ps: with ps; [
    pip
    setuptools
    pyserial
  ]);

  fhsInputs = with pkgs; [
    myPython
    platformio
    pkgsCross.avr.buildPackages.gcc
    avrdude
  ] ++ (if development then [
    nixfmt-rfc-style
    pkgsMaster.vscode
  ] else []);

in
(pkgs.buildFHSEnv {
  name = "solar-switch-env";
  targetPkgs = pkgs: fhsInputs;
  runScript = "bash";
  profile = ''
    export LD_LIBRARY_PATH="${pkgs.stdenv.cc.cc.lib}/lib:$LD_LIBRARY_PATH"
    export PLATFORMIO_CORE_DIR=$PWD/.platformio
    export PYTHONPATH="${myPython}/${myPython.sitePackages}:$PYTHONPATH"
    if [ ${toString development} = "true" ]; then
      code .
    fi

    echo "solar-switch environment is set up!"
  '';
}).env
