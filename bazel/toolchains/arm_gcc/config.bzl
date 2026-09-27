"""cc_toolchain configuration for arm-none-eabi-gcc, parameterized on the Cortex-M core."""

load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES")
load("@rules_cc//cc:cc_toolchain_config_lib.bzl", "feature", "flag_group", "flag_set", "tool_path")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/toolchains:cc_toolchain_config_info.bzl", "CcToolchainConfigInfo")

_TOOLS = {
    "ar": "ar",
    "cpp": "cpp",
    "gcc": "gcc",
    "gcov": "gcov",
    "ld": "gcc",
    "nm": "nm",
    "objcopy": "objcopy",
    "objdump": "objdump",
    "strip": "strip",
}

_COMPILE_ACTIONS = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
    ACTION_NAMES.linkstamp_compile,
]

_LINK_ACTIONS = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

_CPU_FLAGS = {
    "cm4": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb"],
    "cm7": ["-mcpu=cortex-m7", "-mfpu=fpv5-d16", "-mfloat-abi=hard", "-mthumb"],
}

_COMPILE_FLAGS = [
    "-mapcs",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-common",
    "-ffreestanding",
    "-fno-builtin",
]

_CXX_FLAGS = [
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-use-cxa-atexit",
]

# Enabled by bazel according to -c.
_MODE_FLAGS = {
    "dbg": ["-O0", "-g"],
    "opt": ["-O3", "-DNDEBUG"],
}

_LINK_FLAGS = [
    "--specs=nano.specs",
    "--specs=nosys.specs",
    "-Wl,--start-group",
    "-lm",
    "-lc",
    "-lgcc",
    "-lnosys",
    "-Wl,--end-group",
    "-Wl,--gc-sections",
    "-Wl,--print-memory-usage",
    "-static",
    "-Xlinker",
    "-z",
    "-Xlinker",
    "muldefs",
]

def _impl(ctx):
    cpu_flags = _CPU_FLAGS[ctx.attr.core]
    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = "arm-none-eabi-" + ctx.attr.core,
        host_system_name = "local",
        target_system_name = "arm-none-eabi",
        target_cpu = "armv7e-m",
        target_libc = "newlib",
        compiler = "gcc",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = [
            tool_path(name = name, path = ctx.attr.bin_dir + "/arm-none-eabi-" + binary)
            for name, binary in _TOOLS.items()
        ],
        features = [
            _feature("default_compile_flags", _COMPILE_ACTIONS, cpu_flags + _COMPILE_FLAGS),
            _feature("cxx_flags", [ACTION_NAMES.cpp_compile], _CXX_FLAGS),
            _feature("default_link_flags", _LINK_ACTIONS, cpu_flags + _LINK_FLAGS),
        ] + [
            _feature(mode, _COMPILE_ACTIONS, flags, enabled = False)
            for mode, flags in _MODE_FLAGS.items()
        ],
        cxx_builtin_include_directories = ctx.attr.system_include_dirs,
    )

def _feature(name, actions, flags, enabled = True):
    return feature(
        name = name,
        enabled = enabled,
        flag_sets = [flag_set(actions = actions, flag_groups = [flag_group(flags = flags)])],
    )

arm_gcc_toolchain_config = rule(
    implementation = _impl,
    attrs = {
        "core": attr.string(mandatory = True, values = _CPU_FLAGS.keys()),
        "bin_dir": attr.string(mandatory = True),
        "system_include_dirs": attr.string_list(),
    },
    provides = [CcToolchainConfigInfo],
)
