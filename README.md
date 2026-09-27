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
bazel run //test/native:hello-world
bazel run //test/native:hello-udp
```

# Codegen (SVD → register headers)

The `svd_cc_library` macro in `bazel/svd.bzl` runs `forge.svd.RegisterGenerator`
as a Bazel action. Edits to the SVD or to the generator/templates correctly
invalidate the cached output. Example: `test/native/mmio/BUILD.bazel`.

# Consuming forge

```
bazel_dep(name = "forge", version = "0.1.0")
git_override(module_name = "forge", remote = "https://github.com/jacob-heathorn/forge.git", commit = "...")
```

Own code that should be held to forge's warnings uses `forge_cc_library`, `forge_cc_binary` and
`forge_cc_test` from `@forge//bazel:cc.bzl`; they are `cc_library` and friends with the flags from
`copts.bzl` filled in. Repos that develop against a live forge checkout use gordion: `gor bazelrc`
emits the `--override_module` that points bazel at the checkout.

# Repo layout

```
firmware/
  ftl/         header-only template library
  native/      host-side ftl impls (sockets, ethernet)
  pw_unit_test/  vendored Pigweed unit-test framework
  threadx/     header-only ThreadX wrappers (consumed by downstream embedded targets)
test/native/   host gtest + pigweed tests, demo binaries
scripts/package/  forge python package (SVD generator, serial terminal)
bazel/
  cc.bzl       forge_cc_library / forge_cc_binary / forge_cc_test
  copts.bzl    the warning set they apply
  svd.bzl      SVD → register header codegen rule
  third_party/ BUILD files for dependencies without bazel support (ETL)
```

# Copyright & Licensing

Copyright (c) 2025 Jacob Heathorn

This project is released under the **Academic Use License** (see [LICENSE](./LICENSE)).
For **commercial licensing**, please contact: <jacob.heathorn@gmail.com>.
