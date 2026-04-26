"""Generate type-safe peripheral register headers from a CMSIS-SVD file.

The wrapper parses an SVD, hands the result to the model builders in model.py,
then renders the resulting Peripheral / PeripheralFamily objects through the
jinja templates in ./templates/.
"""

import os
import pickle
import textwrap
import time
from contextlib import contextmanager
from pathlib import Path

from cmsis_svd.parser import SVDParser
from jinja2 import Environment, FileSystemLoader

from .model import build_family, build_peripheral


PROJECT_ROOT = Path(os.environ.get("PROJECT_ROOT", ""))
BIN_DIR = PROJECT_ROOT / ".bin"
TEMPLATE_DIR = Path(__file__).parent / "templates"


# =================================================================================================
# Public API

class SVDParserWrapper:
  """Generate ftl::mmio register headers from a CMSIS-SVD XML file."""

  def __init__(self, svd_file, output_dir):
    self.output_dir = Path(output_dir)
    self.device = _load_device(svd_file)
    env = _make_jinja_env()
    self.standalone_template = env.get_template("peripheral.jinja2")
    self.family_template = env.get_template("family.jinja2")

  def generate(self):
    """Regenerate every peripheral header, wiping existing .hpp files first."""
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
      for family in families:
        self._write_family(family)

  def generate_peripheral(self, peripheral_name):
    """Regenerate one peripheral's header. If the peripheral belongs to a
    derivedFrom family the entire family file is regenerated."""
    standalone, families = self._group_peripherals()
    for family in families:
      if any(i.name == peripheral_name for i in family.instances):
        print(f"Rendering family {family.class_name}...")
        print(f"Generated family: {self._write_family(family)}")
        return
    for svd_peripheral in standalone:
      if svd_peripheral.name == peripheral_name:
        print(f"Rendering {svd_peripheral.name}...")
        print(f"Generated: {self._write_standalone(svd_peripheral)}")
        return
    names = [p.name for p in self.device.peripherals]
    print(f"Could not find peripheral {peripheral_name!r}. Options:\n{names}")

  def _write_standalone(self, svd_peripheral):
    peripheral = build_peripheral(svd_peripheral)
    out_path = self.output_dir / f"{peripheral.lower_name}.hpp"
    out_path.write_text(self.standalone_template.render(peripheral=peripheral))
    return out_path

  def _write_family(self, family):
    out_path = self.output_dir / f"{family.lower_name}.hpp"
    out_path.write_text(self.family_template.render(family=family))
    return out_path

  def _group_peripherals(self):
    """Split SVD peripherals into (standalone SVDPeripherals, PeripheralFamilies)."""
    by_canonical = {}
    for p in self.device.peripherals:
      by_canonical.setdefault(p.derived_from or p.name, []).append(p)

    standalone = []
    families = []
    for canonical_name, members in by_canonical.items():
      if len(members) < 2:
        standalone.append(members[0])
        continue
      family, orphans = build_family(canonical_name, members)
      if family is None:
        standalone.extend(members)
        continue
      families.append(family)
      standalone.extend(orphans)
    return standalone, families


# =================================================================================================
# Device loading + jinja environment

def _load_device(svd_file):
  """Parse a CMSIS-SVD file, caching the parsed device alongside .bin/.

  Cache is invalidated when the SVD source is newer — otherwise edits to test
  fixtures (or vendor SVDs) silently use a stale parse.
  """
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


def _make_jinja_env():
  env = Environment(
      loader=FileSystemLoader(str(TEMPLATE_DIR)),
      trim_blocks=True,
      lstrip_blocks=True)
  env.filters["cpp_comment"] = _cpp_comment
  return env


def _cpp_comment(text, width=100, indent=0):
  """Wrap a description into '// ' lines at the given column / indent."""
  collapsed = " ".join((text or "").split())
  if not collapsed:
    return ""
  prefix = " " * indent + "// "
  return "\n".join(textwrap.wrap(
      collapsed, width=width,
      initial_indent=prefix, subsequent_indent=prefix,
      break_long_words=False, break_on_hyphens=False))


@contextmanager
def _phase_timer():
  """Print 'Done (Xs)' when the wrapped block exits."""
  start = time.monotonic()
  yield
  print(f"Done ({time.monotonic() - start:.2f}s)")
