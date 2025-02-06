# Forge

Tooling for forging embedded projects

# Setup

Clone the repo:
```
git clone git@github.com:jheathor-k/forge.git
```

Install direnv:
1. `sudo apt install direnv`
2. Add the following to your .bashrc: `eval "$(direnv hook bash)"`
3. Open a new terminal and change directory to here
4. `direnv allow .`

Install nix:
```
sh <(curl -L https://nixos.org/nix/install) --daemon
```
TODO: Enable nix-direnv I forgot.


Run the setup script: 
```
setup
```

# Repository tests
`pytest`

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
