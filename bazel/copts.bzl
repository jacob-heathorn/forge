"""Warning flags for forge-owned code. Optimization and debug flags belong to the toolchain."""

FORGE_COPTS = [
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wcast-align",
    "-Wformat-security",
    "-Werror=sign-conversion",
    "-Winvalid-pch",
    "-Wmissing-format-attribute",
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

FORGE_CXXOPTS = ["-Wno-interference-size"]
