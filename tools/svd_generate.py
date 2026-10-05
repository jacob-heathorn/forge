"""Writes ftl::mmio register headers for an SVD file: svd_generate <svd_file> <output_dir>."""

import sys
from pathlib import Path

from forge.svd.register_generator import RegisterGenerator


def main() -> None:
  svd, out = sys.argv[1:]
  Path(out).mkdir(parents=True, exist_ok=True)
  RegisterGenerator(svd, out).generate()


if __name__ == "__main__":
  main()
