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
setup -i
```

# Build/run cpp tests

```
rip -c && cmake --workflow --preset native-debug && \
rip -r native-debug:hello-world && \
rip -d native-debug:hello-world

cd /bin/native-release/
ctest

```

# Tests
```
pytest -s
```


# Clean
```
setup -c
rip -c
```
