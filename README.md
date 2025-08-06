# Forge

Tooling for forging embedded projects

# Setup Instructions

1) Clone this repository: `git clone https://github.com/jacob-heathorn/forge.git`
2) Install gordion: `pipx install gordion`
3) Update the gordion dependencies: `gor -u`
4) Install direnv:
  * `sudo apt install direnv`
  * Add the following to your .bashrc: `eval "$(direnv hook bash)"`
  * Open a new terminal and change directory to here.
  * `direnv allow .`
5) Install nix:
  * `sh <(curl -L https://nixos.org/nix/install) --daemon`
6) Install the workspace recommended VSCode extensions.
7) Create the dev environment: `nox -s dev`

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
 