# Forge

Tooling for forging embedded projects

# Setup Instructions

1) Clone this repository: `git clone https://github.com/jacob-heathorn/forge.git`
2) Install direnv:
  * `sudo apt install direnv`
  * Add the following to your .bashrc: `eval "$(direnv hook bash)"`
  * Open a new terminal and change directory to here.
  * `direnv allow .`
3) Install nix:
  * `sh <(curl -L https://nixos.org/nix/install) --daemon`
4) Install vscode extensions:
  * autopep8
  * Better Jinja
  * C/C++
  * Flake8
  * Nix
  * Pylance
  * Python
5) Create the dev environment: `nox -s dev`

# Repository tests
`nox`

# Clean
`rip -c`

# Build
`cmake --workflow --preset native-debug`
`cmake --workflow --preset native-release`

# Run
`rip -r native-debug:hello-world`

# Debug
`rip -d native-debug:hello-world`

# ctest
```
cd /.bin/native-release/
ctest
```

# Hello udp
```
cmake --workflow --preset native-debug && \
rip -r native-debug:hello-udp
```
