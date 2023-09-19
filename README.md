# Forge

Tooling for forging embedded projects

# Setup

Clone the repo:
```
git clone git@github.com:jheathor-k/forge.git
```

You at least need python my broh
```
  sudo apt install python3
  sudo apt install python3-pip
  python3 -m pip install virtualenv
```

Run the setup script to install dependencies: 
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
