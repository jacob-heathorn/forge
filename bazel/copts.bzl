"""Compiler flags for forge-owned targets; external deps are not held to them."""

_BASE_COPTS = [
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wcast-align",
    "-Wformat-security",
    "-Werror=sign-conversion",
    "-Winvalid-pch",
    "-Wmissing-format-attribute",
    "-Wnull-dereference",
    "-Wpacked",
    "-Wpointer-arith",
    "-Wredundant-decls",
    "-Wsign-compare",
    "-Wdouble-promotion",
    "-Wswitch-default",
    "-Wswitch-enum",
    "-Wundef",
    "-Wunused",
    "-Wwrite-strings",
    "-Wshadow",
    "-Wduplicated-cond",
    "-Wmisleading-indentation",
    "-Wunused-but-set-parameter",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-common",
]

FORGE_COPTS = _BASE_COPTS + select({
    "//bazel:dbg": ["-Og", "-ggdb"],
    "//bazel:opt": ["-O3", "-DNDEBUG"],
    "//conditions:default": [],
})

FORGE_CXXOPTS = [
    "-Wno-interference-size",
]
