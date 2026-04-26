"""Module extension that resolves the ETL repository from gordion.

ETL is a gordion-managed dependency. The path to the local checkout is
resolved at repo-rule evaluation time, in this order:

  1. The `ETL_ROOT` environment variable, if set (manual override).
  2. `gordion -f etl` run from the forge workspace, which is the normal
     case once `gordion` has materialized the local checkout.

If neither produces a usable path the build hard-errors — there is
intentionally no upstream-fetch fallback, so mismatched gordion state
surfaces as a real error instead of being papered over.
"""

_ETL_BUILD_FILE = "//bazel:etl.BUILD"

def _resolve_etl_path(repository_ctx):
    override = repository_ctx.os.environ.get("ETL_ROOT")
    if override:
        return override
    result = repository_ctx.execute(
        ["gordion", "-f", "etl"],
        working_directory = str(repository_ctx.workspace_root),
    )
    if result.return_code != 0:
        fail(
            "Could not resolve ETL: ETL_ROOT is unset and `gordion -f etl` " +
            "failed (cwd={}):\n{}".format(
                repository_ctx.workspace_root,
                result.stderr or result.stdout,
            ),
        )
    path = result.stdout.strip()
    if not path:
        fail("`gordion -f etl` returned an empty path — has gordion materialized the checkout?")
    return path

def _gordion_etl_impl(repository_ctx):
    src = _resolve_etl_path(repository_ctx)
    listing = repository_ctx.execute(["ls", "-1A", src])
    if listing.return_code != 0:
        fail("Failed to list ETL path={}: {}".format(src, listing.stderr))
    for entry in listing.stdout.strip().split("\n"):
        if entry and entry != "BUILD.bazel" and entry != "BUILD":
            repository_ctx.symlink(src + "/" + entry, entry)
    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD.bazel")

_gordion_etl = repository_rule(
    implementation = _gordion_etl_impl,
    attrs = {
        "build_file": attr.label(mandatory = True),
    },
    environ = ["ETL_ROOT"],
    local = True,
)

def _etl_impl(_module_ctx):
    _gordion_etl(
        name = "etl",
        build_file = _ETL_BUILD_FILE,
    )

etl = module_extension(
    implementation = _etl_impl,
    environ = ["ETL_ROOT"],
)
