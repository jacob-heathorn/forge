"""Generate ftl::mmio register headers from a CMSIS-SVD file."""

import textwrap
import time
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Optional

from jinja2 import Environment, FileSystemLoader

from .model import Peripheral, PeripheralFamily
from .register_parser import parse


TEMPLATE_DIR = Path(__file__).parent / "templates"


class RegisterGenerator:
  def __init__(self, svd_file: str, output_dir: str) -> None:
    self.output_dir = Path(output_dir)
    with _phase_timer():
      self._standalone, self._families = parse(svd_file)
    env = _make_jinja_env()
    self._peripheral_template = env.get_template("peripheral.jinja2")
    self._family_template = env.get_template("family.jinja2")

  def generate(self) -> None:
    """Wipe existing .hpp files in output_dir, then regenerate all."""
    for hpp in self.output_dir.glob("*.hpp"):
      hpp.unlink()
    total = len(self._standalone) + len(self._families)
    print(f"Generating {total} header(s) "
          f"({len(self._standalone)} standalone, {len(self._families)} families) "
          f"into {self.output_dir}...")
    with _phase_timer():
      for peripheral in self._standalone:
        self._write_peripheral(peripheral)
      for fam in self._families:
        self._write_family(fam)

  def generate_peripheral(self, peripheral_name: str) -> None:
    """Regenerate one peripheral's header. If it's a derivedFrom family
    member, the whole family file is regenerated."""
    for fam in self._families:
      if any(i.name == peripheral_name for i in fam.instances):
        print(f"Rendering family {fam.class_name}...")
        print(f"Generated family: {self._write_family(fam)}")
        return
    for peripheral in self._standalone:
      if peripheral.name == peripheral_name:
        print(f"Rendering {peripheral.name}...")
        print(f"Generated: {self._write_peripheral(peripheral)}")
        return
    names = [p.name for p in self._standalone] + [f.class_name for f in self._families]
    print(f"Could not find peripheral {peripheral_name!r}. Options:\n{names}")

  def _write_peripheral(self, peripheral: Peripheral) -> Path:
    out_path = self.output_dir / f"{peripheral.lower_name}.hpp"
    out_path.write_text(self._peripheral_template.render(peripheral=peripheral))
    return out_path

  def _write_family(self, family: PeripheralFamily) -> Path:
    out_path = self.output_dir / f"{family.lower_name}.hpp"
    out_path.write_text(self._family_template.render(family=family))
    return out_path


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
