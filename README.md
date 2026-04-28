# Forge

Common tooling for embedded projects, including:
* Forge template library (ftl)
* Deployment and debugging tools
* SVD register generators

# Setup

Tested on Ubuntu 24.04.

1. Install bazelisk: `npm i -g @bazel/bazelisk` (or `apt install bazelisk`).
   It auto-fetches the bazel version pinned in `.bazelversion`.
2. Install gordion: `pipx install gordion`.
3. Materialize gordion-managed dependencies: `gor -u`.

That's it. No nix devshell, no direnv, no nox, no cmake.

# Test

```
bazel test //...
```

ETL is resolved automatically via gordion. Set `ETL_ROOT` to override.

# Build

```
bazel build //...                   # default fastbuild
bazel build //... -c dbg            # debug (-Og -ggdb)
bazel build //... -c opt            # release (-O3 -DNDEBUG)
bazel test //... --config=tsan      # ThreadSanitizer
```

# Run a manual binary

```
bazel run //test/native:hello-world
bazel run //test/native:hello-udp
```

# Codegen (SVD → register headers)

The `svd_cc_library` macro in `bazel/svd.bzl` runs `forge.svd.RegisterGenerator`
as a Bazel action. Edits to the SVD or to the generator/templates correctly
invalidate the cached output. Example: `test/native/mmio/BUILD.bazel`.

# Repo layout

```
firmware/
  ftl/         header-only template library
  native/      host-side ftl impls (sockets, ethernet)
  pw_unit_test/  vendored Pigweed unit-test framework
  threadx/     header-only ThreadX wrappers (consumed by downstream embedded targets)
test/native/   host gtest + pigweed tests, demo binaries
scripts/package/  forge python package + rip CLI
bazel/         shared starlark (copts, svd codegen rule, etl extension)
```

# Copyright & Licensing

Copyright (c) 2025 Jacob Heathorn

This project is released under the **Academic Use License** (see [LICENSE](./LICENSE)).
For **commercial licensing**, please contact: <jacob.heathorn@gmail.com>.
