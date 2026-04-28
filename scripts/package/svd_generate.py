"""CLI entry point: run the forge SVD register generator.

Invoked by the Bazel `_svd_headers` rule as: svd_generate <svd_file> <out_dir>.
"""

import sys
from pathlib import Path

from forge.svd.register_generator import RegisterGenerator


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit("usage: svd_generate <svd_file> <output_dir>")
    svd, out = sys.argv[1], sys.argv[2]
    Path(out).mkdir(parents=True, exist_ok=True)
    RegisterGenerator(svd, out).generate()


if __name__ == "__main__":
    main()
