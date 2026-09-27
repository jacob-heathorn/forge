"""Locates arm-none-eabi-gcc on PATH and publishes its location as @arm_gcc//:paths.bzl."""

def _arm_gcc_impl(repository_ctx):
    gcc = repository_ctx.which("arm-none-eabi-gcc")
    bin_dir = str(gcc.dirname) if gcc else ""
    include_dirs = _system_include_dirs(repository_ctx, gcc) if gcc else []
    repository_ctx.file("BUILD.bazel", "")
    repository_ctx.file("paths.bzl", "BIN_DIR = {}\nSYSTEM_INCLUDE_DIRS = {}\n".format(
        repr(bin_dir),
        repr(include_dirs),
    ))

def _system_include_dirs(repository_ctx, gcc):
    probe = repository_ctx.execute([str(gcc), "-E", "-Wp,-v", "-xc++", "/dev/null"])
    dirs = []
    listing = False
    for line in probe.stderr.splitlines():
        if "search starts here:" in line:
            listing = True
        elif "End of search list" in line:
            listing = False
        elif listing:
            dirs.append(line.strip())
    return dirs

_arm_gcc = repository_rule(implementation = _arm_gcc_impl, local = True)

def _arm_gcc_extension_impl(_module_ctx):
    _arm_gcc(name = "arm_gcc")

arm_gcc = module_extension(implementation = _arm_gcc_extension_impl)
