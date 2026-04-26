"""Generate type-safe peripheral register headers from a CMSIS-SVD file.

Output targets the ftl::mmio primitives: no bitfield unions, policy-checked
Register<>. The public surface is the SVDParserWrapper class; everything else
in this module is template support for the jinja files in ./templates/.
"""

import os
import pickle
import re
import textwrap
import time
from contextlib import contextmanager
from pathlib import Path

from cmsis_svd.model import (
    SVDAccessType,
    SVDRegister,
    SVDRegisterArray,
    SVDRegisterClusterArray,
)
from cmsis_svd.parser import SVDParser
from jinja2 import Environment, FileSystemLoader


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
    self.standalone_template = env.get_template("cmsis_svd_registers.jinja2")
    self.family_template = env.get_template("cmsis_svd_family.jinja2")

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
      for peripheral in standalone:
        self._write_standalone(peripheral)
      for family in families:
        self._write_family(family)

  def generate_peripheral(self, peripheral_name):
    """Regenerate one peripheral's header. If the peripheral belongs to a
    derivedFrom family the entire family file is regenerated. Other
    peripherals' headers are left alone."""
    standalone, families = self._group_peripherals()
    for family in families:
      if any(p.name == peripheral_name for _, p in family["instances"]):
        print(f"Rendering family {family['class_name']}...")
        print(f"Generated family: {self._write_family(family)}")
        return
    for peripheral in standalone:
      if peripheral.name == peripheral_name:
        print(f"Rendering {peripheral.name}...")
        print(f"Generated: {self._write_standalone(peripheral)}")
        return
    names = [p.name for p in self.device.peripherals]
    print(f"Could not find peripheral {peripheral_name!r}. Options:\n{names}")

  def _write_standalone(self, peripheral):
    _validate_peripheral(peripheral)
    return self._render(
        self.standalone_template,
        f"{peripheral.name.lower()}.hpp",
        peripheral=peripheral)

  def _write_family(self, family):
    _validate_peripheral(family["parent"])
    return self._render(
        self.family_template,
        f"{family['family_name'].lower()}.hpp",
        family=family,
        peripheral=family["parent"])

  def _render(self, template, filename, **context):
    out_path = self.output_dir / filename
    out_path.write_text(template.render(**context))
    return out_path

  def _group_peripherals(self):
    """Split peripherals into standalone and derivedFrom families.

    Returns (standalone, families). Each family is a dict:
      parent       — canonical SVDPeripheral whose layout is shared
      family_name  — common prefix (e.g. "GPIO" for GPIO1..GPIO13)
      class_name   — capitalized C++ struct name
      instances    — sorted [(instance_number, SVDPeripheral), ...]
    """
    by_canonical = {}
    for p in self.device.peripherals:
      by_canonical.setdefault(p.derived_from or p.name, []).append(p)

    standalone = []
    families = []
    for canonical_name, members in by_canonical.items():
      if len(members) < 2:
        standalone.append(members[0])
        continue
      family_name = re.sub(r"\d+$", "", canonical_name) or canonical_name
      instances, orphans = _split_family_members(members, family_name)
      if len(instances) < 2:
        standalone.extend(members)
        continue
      standalone.extend(orphans)
      instances.sort(key=lambda kv: kv[0])
      parent = next((p for p in members if p.name == canonical_name), members[0])
      families.append({
        "parent": parent,
        "family_name": family_name,
        "class_name": family_name.capitalize(),
        "instances": instances,
      })
    return standalone, families


# =================================================================================================
# Device loading + jinja environment

def _load_device(svd_file):
  """Parse a CMSIS-SVD file, caching the parsed device alongside .bin/.

  The cache is invalidated when the SVD source is newer — otherwise edits to
  test fixtures (or vendor SVDs) silently use a stale parse.
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


@contextmanager
def _phase_timer():
  """Print 'Done (Xs)' when the wrapped block exits."""
  start = time.monotonic()
  yield
  print(f"Done ({time.monotonic() - start:.2f}s)")


def _make_jinja_env():
  env = Environment(loader=FileSystemLoader(str(TEMPLATE_DIR)))
  env.globals.update(
      format_comment=format_comment,
      normalize_register_name=normalize_register_name,
      register_size=register_size,
      register_storage_type=register_storage_type,
      register_reset_literal=register_reset_literal,
      build_field_rows=build_field_rows,
      mmio_access=mmio_access,
      mmio_modify_write=mmio_modify_write,
      peripheral_render_items=peripheral_render_items,
      SVDAccessType=SVDAccessType,
  )
  return env


# =================================================================================================
# Validation + family grouping

_SUPPORTED_REGISTER_TYPES = (SVDRegister, SVDRegisterArray, SVDRegisterClusterArray)


def _validate_peripheral(peripheral):
  for register in peripheral.registers:
    if not isinstance(register, _SUPPORTED_REGISTER_TYPES):
      raise NotImplementedError(
          f"Peripheral {peripheral.name!r} contains a register "
          f"{getattr(register, 'name', '<unnamed>')!r} of type "
          f"{type(register).__name__} which the generator does not yet emit. "
          f"Add support to peripheral_render_items() + the jinja templates, "
          f"plus a fixture in forge/test/native/mmio/test_peripheral.svd, "
          f"before regenerating.")


def _split_family_members(members, family_name):
  """Partition family members into digit-suffixed instances and orphans."""
  instances, orphans = [], []
  for m in members:
    suffix = m.name[len(family_name):]
    if suffix.isdigit():
      instances.append((int(suffix), m))
    else:
      orphans.append(m)
  return instances, orphans


# =================================================================================================
# Render-item construction (called from jinja via peripheral_render_items)

def peripheral_render_items(peripheral, *, base_expr=None):
  """Flatten a peripheral's registers/arrays/clusters into uniform render items.

  Each item is a dict with {register, name, addr_expr, is_templated, template_params}.
  base_expr controls address emission:
    None      -> absolute addresses (standalone peripheral, namespace shape)
    'kBase'   -> '{base_expr} + offset' (peripheral family — kBase per Instance)
  """
  for register in peripheral.registers:
    yield from _items_for_register(register, peripheral, base_expr)


def _items_for_register(register, peripheral, base_expr):
  if isinstance(register, SVDRegister):
    yield _make_render_item(
        register, register.name, register.address_offset,
        peripheral, base_expr, [])
  elif isinstance(register, SVDRegisterArray):
    proto = register.registers[0]
    meta = register.meta_register
    yield _make_render_item(
        proto, _strip_dim_placeholder(meta.name), proto.address_offset,
        peripheral, base_expr,
        [_template_param("Index", len(register.registers), meta.dim_increment)])
  elif isinstance(register, SVDRegisterClusterArray):
    yield from _items_for_cluster_array(register, peripheral, base_expr)


def _items_for_cluster_array(cluster_array, peripheral, base_expr):
  cluster_proto = cluster_array.clusters[0]
  cluster_param = _template_param(
      "ClusterIndex", len(cluster_array.clusters), cluster_proto.dim_increment)
  prefix = cluster_proto.name + "_"

  def strip_prefix(name):
    return name[len(prefix):] if name.startswith(prefix) else name

  for inner in cluster_proto.registers:
    if isinstance(inner, SVDRegister):
      offset = _undouble_count_offset(inner.address_offset, cluster_proto)
      yield _make_render_item(
          inner, strip_prefix(inner.name), offset,
          peripheral, base_expr, [cluster_param])
    elif isinstance(inner, SVDRegisterArray):
      proto = inner.registers[0]
      meta = inner.meta_register
      offset = _undouble_count_offset(proto.address_offset, cluster_proto)
      yield _make_render_item(
          proto, strip_prefix(_strip_dim_placeholder(meta.name)), offset,
          peripheral, base_expr,
          [cluster_param,
           _template_param("ArrayIndex", len(inner.registers), meta.dim_increment)])
    else:
      raise NotImplementedError(
          f"Peripheral {peripheral.name!r} cluster {cluster_proto.name!r} "
          f"contains an inner element of type {type(inner).__name__} which is not "
          f"yet supported. Extend peripheral_render_items() and add a fixture in "
          f"forge/test/native/mmio/test_peripheral.svd.")


def _make_render_item(register, name, offset, peripheral, base_expr, template_params):
  return {
    "register": register,
    "name": normalize_register_name(name),
    "addr_expr": _format_addr_expr(peripheral, offset, base_expr, template_params),
    "is_templated": bool(template_params),
    "template_params": template_params,
  }


def _format_addr_expr(peripheral, offset, base_expr, template_params):
  """Build the address expression substituted into Register<>'s template.

  Standalone peripherals (base_expr is None) get an absolute hex literal.
  Peripheral families get '{base_expr} + offset'; the enclosing struct supplies
  kBase per Instance. Both forms can carry an extra ' + (P * inc)' tail for
  cluster/array index strides.
  """
  if base_expr is None:
    expr = f"0x{peripheral.base_address + offset:08X}u"
  else:
    expr = f"{base_expr} + 0x{offset:X}u"
  for tp in template_params:
    expr += f" + ({tp['name']} * 0x{tp['increment']:X}u)"
  return expr


def _template_param(name, dim, increment):
  return {"name": name, "dim": dim, "increment": increment}


def _undouble_count_offset(inner_offset, container):
  """Some vendor SVDs (e.g. NXP MIMXRT DMA TCDs) put peripheral-relative
  offsets on cluster-inner registers instead of cluster-relative. cmsis-svd
  then uniformly adds the container offset, double-counting. Detect when the
  resolved offset lands past the container's range and undo it."""
  if inner_offset >= container.address_offset + container.dim_increment:
    return inner_offset - container.address_offset
  return inner_offset


def _strip_dim_placeholder(name):
  """Remove the [%s] / %s SVD <dim> placeholder from an array name."""
  return name.replace("[%s]", "").replace("%s", "")


# =================================================================================================
# Field-row construction (called from jinja via build_field_rows)

def build_field_rows(register):
  """Ordered rows covering every bit of the register: field rows for SVD
  <field>s, reserved rows filling the gaps. The template emits Field<...> for
  one and Reserved<...> for the other."""
  reg_bits = register_size(register)
  rows = []
  cursor = 0
  for field in sorted(register.fields, key=lambda f: f.bit_offset):
    if field.bit_offset > cursor:
      rows.append(_reserved_row(cursor, field.bit_offset - cursor))
    rows.append(_field_row(field, register))
    cursor = field.bit_offset + field.bit_width
  if cursor < reg_bits:
    rows.append(_reserved_row(cursor, reg_bits - cursor))
  return rows


def _field_row(field, register):
  return {
    "kind": "field",
    "name": field.name,
    "offset": field.bit_offset,
    "width": field.bit_width,
    "field": field,
    "value_type": field_value_type(field),
    "access": mmio_access(field.access),
    "modify": mmio_modify_write(
        field.modified_write_values,
        field_name=field.name,
        register_name=register.name),
  }


def _reserved_row(offset, width):
  return {"kind": "reserved", "offset": offset, "width": width}


# =================================================================================================
# SVD attribute → C++ / ftl::mmio mappings

_ACCESS_TO_MMIO = {
    SVDAccessType.READ_ONLY:       "ftl::mmio::RO",
    SVDAccessType.WRITE_ONLY:      "ftl::mmio::WO",
    SVDAccessType.WRITE_ONCE:      "ftl::mmio::WO",
    SVDAccessType.READ_WRITE:      "ftl::mmio::RW",
    SVDAccessType.READ_WRITE_ONCE: "ftl::mmio::RW",
}


def mmio_access(access):
  """Map SVDAccessType to ftl::mmio access tag. Unspecified → RW (SVD default)."""
  return _ACCESS_TO_MMIO.get(access, "ftl::mmio::RW")


_MODIFY_WRITE_TO_MMIO = {
    "ONE_TO_CLEAR":  "ftl::mmio::OneToClear",
    "ONE_TO_SET":    "ftl::mmio::OneToSet",
    "ONE_TO_TOGGLE": "ftl::mmio::OneToToggle",
    "MODIFY":        "ftl::mmio::Normal",
}


def mmio_modify_write(mwv, *, field_name=None, register_name=None):
  """Map SVDModifiedWriteValuesType to an ftl::mmio modify-write tag.

  Fails loud on values ftl::mmio::Register::modify() can't safely handle yet
  (ZERO_TO_*, CLEAR, SET) so we don't silently emit headers that misbehave on
  RMW. To support a new value: extend ftl::mmio + add a fixture in
  forge/test/native/mmio/test_peripheral.svd before regenerating.
  """
  if mwv is None:
    return "ftl::mmio::Normal"
  name = getattr(mwv, "name", str(mwv))
  if name in _MODIFY_WRITE_TO_MMIO:
    return _MODIFY_WRITE_TO_MMIO[name]
  where = ""
  if register_name and field_name:
    where = f" (register {register_name}, field {field_name})"
  raise NotImplementedError(
      f"<modifiedWriteValues>{name.lower()}</modifiedWriteValues>{where} is "
      f"not supported by ftl::mmio. "
      f"Supported values: {sorted(_MODIFY_WRITE_TO_MMIO)}.")


def field_value_type(field):
  """C++ type name used for a field's value: enum class for enumerated fields,
  else the smallest unsigned int that holds the field width (or bool for w==1)."""
  if field.is_enumerated_type and field.enumerated_values and len(field.enumerated_values) == 1:
    enum_set = field.enumerated_values[0]
    return f"e{enum_set.name or field.name}"
  width = field.bit_width
  if width == 1:  return "bool"
  if width <= 8:  return "std::uint8_t"
  if width <= 16: return "std::uint16_t"
  return "std::uint32_t"


def register_size(register):
  """Bit width of a register (default 32 if unspecified)."""
  return register.size if register.size is not None else 32


def register_storage_type(register):
  """C++ unsigned storage type matching the register's bit width."""
  bits = register_size(register)
  if bits <= 8:  return "std::uint8_t"
  if bits <= 16: return "std::uint16_t"
  return "std::uint32_t"


def register_reset_literal(register):
  """Reset-value literal sized to match the register's storage type."""
  bits = register_size(register)
  reset = register.reset_value or 0
  hex_digits = {8: 2, 16: 4}.get(bits, 8)
  return f"0x{reset:0{hex_digits}X}u"


def normalize_register_name(name):
  """'FOO[3]' → 'FOO_3' for array-element registers."""
  match = re.fullmatch(r"(.+)\[(\d+)\]", name)
  return f"{match.group(1)}_{match.group(2)}" if match else name


def format_comment(comment, width=100, indent_width=0):
  """Wrap a comment string into '// ' lines at the given column/indent."""
  text = " ".join((comment or "").split())
  if not text:
    return ""
  prefix = " " * indent_width + "// "
  return "\n".join(textwrap.wrap(
      text, width=width,
      initial_indent=prefix, subsequent_indent=prefix,
      break_long_words=False, break_on_hyphens=False))
