"""Module extension that resolves the ETL repository from gordion.

ETL is a gordion-managed dependency. The ETL_ROOT environment variable must
point at the local checkout — this is the only supported way to provide ETL
to the build. There is no upstream fetch fallback; mismatched gordion state
should surface as a hard error rather than be papered over.
"""

_ETL_BUILD_FILE = "//bazel:etl.BUILD"

def _local_etl_impl(repository_ctx):
    src = repository_ctx.attr.path
    listing = repository_ctx.execute(["ls", "-1A", src])
    if listing.return_code != 0:
        fail("Failed to list ETL_ROOT={}: {}".format(src, listing.stderr))
    for entry in listing.stdout.strip().split("\n"):
        if entry and entry != "BUILD.bazel" and entry != "BUILD":
            repository_ctx.symlink(src + "/" + entry, entry)
    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD.bazel")

_local_etl = repository_rule(
    implementation = _local_etl_impl,
    attrs = {
        "path": attr.string(mandatory = True),
        "build_file": attr.label(mandatory = True),
    },
    local = True,
)

def _etl_impl(module_ctx):
    etl_root = module_ctx.os.environ.get("ETL_ROOT")
    if not etl_root:
        fail("ETL_ROOT is not set. ETL is provided by gordion — run gordion " +
             "to materialize the local checkout, then export ETL_ROOT before " +
             "invoking bazel.")
    _local_etl(
        name = "etl",
        path = etl_root,
        build_file = _ETL_BUILD_FILE,
    )

etl = module_extension(
    implementation = _etl_impl,
    environ = ["ETL_ROOT"],
)
