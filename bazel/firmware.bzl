"""Firmware images: an ELF built for one core, its flat binary, and a C++ array for embedding it."""

load("@rules_cc//cc:defs.bzl", "cc_binary")

def firmware_image(name, platform, visibility = None, **kwargs):
    """Defines name.elf, name.bin and name.bin.cpp, built for one core from any configuration.

    Args:
      name: Image name; also the alias for name.elf.
      platform: Platform label the image is built for, e.g. //bazel/platforms:cm7.
      visibility: Visibility of the outputs.
      **kwargs: Forwarded to the underlying cc_binary.
    """
    elf = "_" + name + ".elf"
    bin = "_" + name + ".bin"
    cpp = "_" + name + ".bin.cpp"
    cc_binary(name = elf, tags = ["manual"], **kwargs)
    _elf_to_bin(name = bin, elf = elf, tags = ["manual"])
    _bin_to_cpp(name = cpp, bin = bin, symbol = name.replace("-", "_") + "_bin", tags = ["manual"])
    for artifact in [elf, bin, cpp]:
        platform_transition(
            name = artifact[1:],
            target = artifact,
            platform = platform,
            visibility = visibility,
        )
    native.alias(name = name, actual = name + ".elf", visibility = visibility)

def _platform_transition_impl(_settings, attr):
    return {"//command_line_option:platforms": str(attr.platform)}

_platform_transition = transition(
    implementation = _platform_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

def _platform_transition_rule_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.name)
    ctx.actions.symlink(output = out, target_file = ctx.file.target)
    return [DefaultInfo(files = depset([out]))]

# Rebuilds a single-file target for the given platform, whatever the consumer's configuration.
platform_transition = rule(
    implementation = _platform_transition_rule_impl,
    attrs = {
        "target": attr.label(cfg = _platform_transition, allow_single_file = True, mandatory = True),
        "platform": attr.label(mandatory = True),
    },
)

def _elf_to_bin_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.name)
    cc = ctx.toolchains["@bazel_tools//tools/cpp:toolchain_type"].cc
    ctx.actions.run(
        executable = cc.objcopy_executable,
        arguments = ["-Obinary", ctx.file.elf.path, out.path],
        inputs = [ctx.file.elf],
        tools = cc.all_files,
        outputs = [out],
        mnemonic = "ObjCopy",
    )
    return [DefaultInfo(files = depset([out]))]

_elf_to_bin = rule(
    implementation = _elf_to_bin_impl,
    attrs = {"elf": attr.label(allow_single_file = True, mandatory = True)},
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)

def _bin_to_cpp_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.name)
    ctx.actions.run(
        executable = ctx.executable._bin_to_cpp,
        arguments = [ctx.attr.symbol, ctx.file.bin.path, out.path],
        inputs = [ctx.file.bin],
        outputs = [out],
        mnemonic = "BinToCpp",
    )
    return [DefaultInfo(files = depset([out]))]

_bin_to_cpp = rule(
    implementation = _bin_to_cpp_impl,
    attrs = {
        "bin": attr.label(allow_single_file = True, mandatory = True),
        "symbol": attr.string(mandatory = True),
        "_bin_to_cpp": attr.label(
            default = "//bazel/tools:bin_to_cpp",
            executable = True,
            cfg = "exec",
        ),
    },
)
