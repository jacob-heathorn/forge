"""SVD → C++ register header codegen, wired as proper Bazel actions.

`svd_cc_library(name, svd, deps)` runs the forge code generator on the given
SVD file and exposes the resulting headers as a cc_library that downstream
targets can depend on. Edits to the SVD or to any generator source file
correctly invalidate the cached output.
"""

load("@rules_cc//cc:defs.bzl", "cc_library")

def _svd_headers_impl(ctx):
    out_dir = ctx.actions.declare_directory(ctx.label.name)
    ctx.actions.run(
        outputs = [out_dir],
        inputs = [ctx.file.svd],
        executable = ctx.executable._generator,
        arguments = [ctx.file.svd.path, out_dir.path],
        progress_message = "Generating MMIO headers from %{input}",
        mnemonic = "SvdGen",
    )
    return [DefaultInfo(files = depset([out_dir]))]

_svd_headers = rule(
    implementation = _svd_headers_impl,
    attrs = {
        "svd": attr.label(allow_single_file = [".svd"], mandatory = True),
        "_generator": attr.label(
            default = "//scripts/package:svd_generate",
            executable = True,
            cfg = "exec",
        ),
    },
)

def svd_cc_library(name, svd, deps = [], visibility = None):
    headers = name + "_headers"
    _svd_headers(name = headers, svd = svd)
    cc_library(
        name = name,
        hdrs = [":" + headers],
        includes = [headers],
        deps = deps,
        visibility = visibility,
    )
