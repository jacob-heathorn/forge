{
  description = "Forge native development flake";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
    gtestPath = builtins.toString pkgs.gtest.dev.outPath;
    gccArmPath = builtins.toString pkgs.gcc-arm-embedded-13;
    repo = builtins.toString ./.;
  in {
    devShells.${system}.default = pkgs.mkShell {
      buildInputs = [
        pkgs.cmake
        pkgs.uv
        pkgs.glibcLocales
        pkgs.gtest
        pkgs.gcc-arm-embedded-13
        pkgs.python312Packages.flake8
        pkgs.python312Packages.mypy
        pkgs.python312Packages.nox
      ];

      shellHook = ''
        export CMAKE_PREFIX_PATH=${gtestPath}
        export GTEST_INCLUDE_DIR=${gtestPath}/include
        export PYTHONPYCACHEPREFIX=$PROJECT_ROOT/.pycache
        export ARM_GCC_TOOLCHAIN_PATH=${gccArmPath}/bin
        echo -e "\033[1;32mWelcome to the forge development shell!\033[0m"
        source ${repo}/scripts/common.envrc
      '';
    };
  };
}
