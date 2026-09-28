"""Generate ftl::mmio register headers from a CMSIS-SVD file."""

import textwrap
from pathlib import Path
from typing import Optional

from jinja2 import Environment, FileSystemLoader

from .model import Peripheral, PeripheralFamily
from .register_parser import parse


TEMPLATE_DIR = Path(__file__).parent / "templates"


class RegisterGenerator:
  def __init__(self, svd_file: str, output_dir: str) -> None:
    self.output_dir = Path(output_dir)
    self._standalone, self._families = parse(svd_file)
    env = _make_jinja_env()
    self._peripheral_template = env.get_template("peripheral.jinja2")
    self._family_template = env.get_template("family.jinja2")

  def generate(self) -> None:
    """Writes one header per standalone peripheral and per derivedFrom family."""
    for peripheral in self._standalone:
      path = self.output_dir / f"{peripheral.lower_name}.hpp"
      path.write_text(self._peripheral_template.render(peripheral=peripheral))
    for family in self._families:
      path = self.output_dir / f"{family.lower_name}.hpp"
      path.write_text(self._family_template.render(family=family))


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
