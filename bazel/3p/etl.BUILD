# BUILD file for the etl archive; MODULE.bazel installs it at the archive's root.

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "etl",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
