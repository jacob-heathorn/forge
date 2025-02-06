{
  description = "Default native development shell flake";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }: {
    devShells = {
      x86_64-linux = {
        default = with import nixpkgs { system = "x86_64-linux"; };
        mkShell {
          buildInputs = [
          pkgs.ansible
          pkgs.cmake
          pkgs.poetry
          pkgs.glibcLocales
          pkgs.gtest
          pkgs.gcc-arm-embedded-13
        ];

        shellHook = ''
          export PYTHONPYCACHEPREFIX=$FORGE_ROOT/.pycache
          echo -e "\033[1;32mWelcome to your linux development shell!\033[0m"
        '';
        };
      };
    };
  };
}
