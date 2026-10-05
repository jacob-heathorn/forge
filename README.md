# Forge

Shared building blocks for embedded C++ projects: the ftl template library, SVD register
codegen, and bazel rules.

## Setup

Tested on Ubuntu 24.04. Install bazelisk (`npm i -g @bazel/bazelisk`); bazel fetches everything
else.

## Test

```
bazel test //...
bazel test //... --config=tsan     # under ThreadSanitizer
```

## Run a demo

```
bazel run //apps:hello_world
bazel run //apps:hello_udp
```

## Debug and release

Add `-c dbg` or `-c opt` to any command for a debug or release build:

```
bazel test -c opt //...
```

## License

Copyright (c) 2025 Jacob Heathorn. Released under the **Academic Use License**, see
[LICENSE](./LICENSE). For commercial licensing contact <jacob.heathorn@gmail.com>.
