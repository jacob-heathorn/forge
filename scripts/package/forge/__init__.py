# flake8: noqa: F401
# Lazy module: only re-exports the SVD generator, which is the one piece the
# Bazel build invokes at codegen time. Tooling-side helpers (debugger, vscode,
# serial_terminal) are imported from their submodules directly to avoid
# pulling in their heavyweight deps when the build only needs codegen.
from .svd import RegisterGenerator
