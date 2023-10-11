# Forge

Tooling for forging embedded projects

# Setup

Clone the repo:
```
git clone git@github.com:jheathor-k/forge.git
```

Install python3
```
  sudo apt install python3
  sudo apt install python3-pip
  python3 -m pip install virtualenv
```

Install direnv
1. `sudo apt install direnv`
2. Add the following to your .bashrc: `eval "$(direnv hook bash)"`
3. Open a new terminal and change directory to here
4. `direnv allow .`


Run the setup script to install other dependencies: 
```
setup -i
```

# Build/run cpp tests

```
rip -b -r hello-world

cd /bin/native-release/
ctest

```

# Clean
```
setup -c
rip -c
```
