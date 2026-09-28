# Forge

Common tooling for embedded projects, including:
* Forge template library (ftl)
* Serial terminal for flash-and-run tooling
* SVD register generators
* Shared bazel rules: forge_cc_* with forge's warning set, SVD register codegen

# Setup

Tested on Ubuntu 24.04.

Install bazelisk: `npm i -g @bazel/bazelisk` (or `apt install bazelisk`).
It fetches the bazel version pinned in `.bazelversion`; bazel fetches everything else.

# Test

```
bazel test //...
```

# Build

```
bazel build //...                   # default fastbuild
bazel build //... -c dbg            # debug (-Og -ggdb)
bazel build //... -c opt            # release (-O3 -DNDEBUG)
bazel test //... --config=tsan      # ThreadSanitizer
```

# Run a manual binary

```
bazel run //apps:hello_world
bazel run //apps:hello_udp
```

# Codegen (SVD → register headers)

The `svd_cc_library` macro in `bazel/svd.bzl` runs `forge.svd.RegisterGenerator`
as a Bazel action. Edits to the SVD or to the generator/templates correctly
invalidate the cached output. Example: `forge/svd/BUILD.bazel`.

# Consuming forge

Add `bazel_dep(name = "forge", version = "0.1.0")` and list forge in `gordion.yaml`; `gor bazelrc`
points bazel at gordion's checkout. Own code that should be held to forge's warnings uses
`forge_cc_library`, `forge_cc_binary` and `forge_cc_test` from `@forge//bazel:cc.bzl`; they are
`cc_library` and friends with the flags from `copts.bzl` filled in.

# Repo layout

Headers, sources and tests live together; a header's include path is its repo path, e.g.
`#include "forge/ftl/map.hpp"`.

```
forge/
  ftl/           header-only template library and its tests
  native/        host implementations of ftl's thread and network interfaces
  threadx/       ThreadX implementations of the same (header-only)
  pw_unit_test/  vendored Pigweed unit-test framework
  svd/           test of the SVD register codegen
apps/            host demo binaries
tools/           forge python package: SVD generator, serial terminal
bazel/
  cc.bzl       forge_cc_library / forge_cc_binary / forge_cc_test
  copts.bzl    the warning set they apply
  svd.bzl      SVD → register header codegen rule
  3p/          BUILD files for dependencies without bazel support (ETL)
```

# Copyright & Licensing

Copyright (c) 2025 Jacob Heathorn

This project is released under the **Academic Use License** (see [LICENSE](./LICENSE)).
For **commercial licensing**, please contact: <jacob.heathorn@gmail.com>.
