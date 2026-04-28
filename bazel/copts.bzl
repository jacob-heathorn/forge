# Shared C/C++ flags applied to forge-owned targets only. External deps
# (gtest, etl) intentionally do not inherit these — they don't need to pass
# -Werror under our warning set.

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

# Per-compilation-mode flags. CMake's release preset is -O3 -DNDEBUG, debug is
# -Og -ggdb; mirror that here so `-c opt` and `-c dbg` match the old presets.
FORGE_COPTS = _BASE_COPTS + select({
    "//bazel:dbg": ["-Og", "-ggdb"],
    "//bazel:opt": ["-O3", "-DNDEBUG"],
    "//conditions:default": [],
})

# C++-only flags (avoid passing to C compilations of any vendored .c).
FORGE_CXXOPTS = [
    "-Wno-interference-size",
]
