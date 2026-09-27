"""Downloads the Arm GNU Toolchain and defines an arm-none-eabi-gcc cc_toolchain per core."""

_VERSION = "13.3.rel1"
_GCC_VERSION = "13.3.1"
_ARCHIVE = "arm-gnu-toolchain-{}-x86_64-arm-none-eabi".format(_VERSION)
_URL = "https://developer.arm.com/-/media/Files/downloads/gnu/{}/binrel/{}.tar.xz".format(_VERSION, _ARCHIVE)
_SHA256 = "95c011cee430e64dd6087c75c800f04b9c49832cc1000127a92a97f9c8d83af4"

_INCLUDE_DIRS = [
    "arm-none-eabi/include",
    "arm-none-eabi/include/c++/" + _GCC_VERSION,
    "arm-none-eabi/include/c++/{}/arm-none-eabi".format(_GCC_VERSION),
    "arm-none-eabi/include/c++/{}/backward".format(_GCC_VERSION),
    "lib/gcc/arm-none-eabi/{}/include".format(_GCC_VERSION),
    "lib/gcc/arm-none-eabi/{}/include-fixed".format(_GCC_VERSION),
]

def _arm_gcc_impl(repository_ctx):
    repository_ctx.download_and_extract(url = _URL, sha256 = _SHA256, stripPrefix = _ARCHIVE)
    include_dirs = [str(repository_ctx.path(dir)) for dir in _INCLUDE_DIRS]
    repository_ctx.template("BUILD.bazel", repository_ctx.attr._build, {
        "%{include_dirs}": repr(include_dirs),
    })

_arm_gcc = repository_rule(
    implementation = _arm_gcc_impl,
    attrs = {"_build": attr.label(default = ":BUILD.arm_gcc")},
)

def _arm_gcc_extension_impl(_module_ctx):
    _arm_gcc(name = "arm_gcc")

arm_gcc = module_extension(implementation = _arm_gcc_extension_impl)
