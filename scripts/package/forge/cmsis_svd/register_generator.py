"""Generate ftl::mmio register headers from a CMSIS-SVD file."""

import os
import pickle
import textwrap
import time
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Iterator, Optional

from cmsis_svd.parser import SVDParser
from jinja2 import Environment, FileSystemLoader

from .register_parser import family, PeripheralFamily, standalone_peripheral


PROJECT_ROOT = Path(os.environ.get("PROJECT_ROOT", ""))
BIN_DIR = PROJECT_ROOT / ".bin"
TEMPLATE_DIR = Path(__file__).parent / "templates"


class RegisterGenerator:
  def __init__(self, svd_file: str, output_dir: str) -> None:
    self.output_dir = Path(output_dir)
    self.device = _load_device(svd_file)
    env = _make_jinja_env()
    self.standalone_template = env.get_template("peripheral.jinja2")
    self.family_template = env.get_template("family.jinja2")

  def generate(self) -> None:
    """Wipe existing .hpp files in output_dir, then regenerate all."""
    for hpp in self.output_dir.glob("*.hpp"):
      hpp.unlink()
    standalone, families = self._group_peripherals()
    total = len(standalone) + len(families)
    print(f"Generating {total} header(s) "
          f"({len(standalone)} standalone, {len(families)} families) "
          f"into {self.output_dir}...")
    with _phase_timer():
      for svd_peripheral in standalone:
        self._write_standalone(svd_peripheral)
      for fam in families:
        self._write_family(fam)

  def generate_peripheral(self, peripheral_name: str) -> None:
    """Regenerate one peripheral's header. If it's a derivedFrom family
    member, the whole family file is regenerated."""
    standalone, families = self._group_peripherals()
    for fam in families:
      if any(i.name == peripheral_name for i in fam.instances):
        print(f"Rendering family {fam.class_name}...")
        print(f"Generated family: {self._write_family(fam)}")
        return
    for svd_peripheral in standalone:
      if svd_peripheral.name == peripheral_name:
        print(f"Rendering {svd_peripheral.name}...")
        print(f"Generated: {self._write_standalone(svd_peripheral)}")
        return
    names = [p.name for p in self.device.peripherals]
    print(f"Could not find peripheral {peripheral_name!r}. Options:\n{names}")

  def _write_standalone(self, svd_peripheral: Any) -> Path:
    peripheral = standalone_peripheral(svd_peripheral)
    out_path = self.output_dir / f"{peripheral.lower_name}.hpp"
    out_path.write_text(self.standalone_template.render(peripheral=peripheral))
    return out_path

  def _write_family(self, fam: PeripheralFamily) -> Path:
    out_path = self.output_dir / f"{fam.lower_name}.hpp"
    out_path.write_text(self.family_template.render(family=fam))
    return out_path

  def _group_peripherals(self) -> tuple[list[Any], list[PeripheralFamily]]:
    by_canonical: dict[str, list[Any]] = {}
    for p in self.device.peripherals:
      by_canonical.setdefault(p.derived_from or p.name, []).append(p)

    standalone: list[Any] = []
    families: list[PeripheralFamily] = []
    for canonical_name, members in by_canonical.items():
      if len(members) < 2:
        standalone.append(members[0])
        continue
      fam, orphans = family(canonical_name, members)
      if fam is None:
        standalone.extend(members)
        continue
      families.append(fam)
      standalone.extend(orphans)
    return standalone, families


def _load_device(svd_file: str) -> Any:
  # Cache invalidation by mtime: edits to test fixtures or vendor SVDs would
  # otherwise silently reuse a stale parse.
  cache_path = BIN_DIR / f"{Path(svd_file).stem}.pkl"
  if cache_path.exists() and cache_path.stat().st_mtime >= os.path.getmtime(svd_file):
    print(f"Loading cached device from {cache_path}...")
    with _phase_timer(), cache_path.open("rb") as f:
      return pickle.load(f)

  print(f"Parsing {svd_file}...")
  with _phase_timer():
    device = SVDParser.for_xml_file(svd_file).get_device()
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    with cache_path.open("wb") as f:
      pickle.dump(device, f)
  return device


def _make_jinja_env() -> Environment:
  env = Environment(
      loader=FileSystemLoader(str(TEMPLATE_DIR)),
      trim_blocks=True,
      lstrip_blocks=True)
  env.filters["cpp_comment"] = _cpp_comment
  return env


def _cpp_comment(text: Optional[str], width: int = 100, indent: int = 0) -> str:
  collapsed = " ".join((text or "").split())
  if not collapsed:
    return ""
  prefix = " " * indent + "// "
  return "\n".join(textwrap.wrap(
      collapsed, width=width,
      initial_indent=prefix, subsequent_indent=prefix,
      break_long_words=False, break_on_hyphens=False))


@contextmanager
def _phase_timer() -> Iterator[None]:
  start = time.monotonic()
  yield
  print(f"Done ({time.monotonic() - start:.2f}s)")
