# System python modules
import os
import pickle
from jinja2 import Environment, FileSystemLoader

# https://github.com/cmsis-svd/cmsis-svd
from cmsis_svd.parser import SVDParser
from cmsis_svd.model import SVDAccessType

FORGE_ROOT = os.environ.get("FORGE_ROOT", "")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT", "")
BIN_DIR = os.path.join(PROJECT_ROOT, '.bin')


# =================================================================================================
# Helpers

def clean_headers(dir: str):
  """Delete every .hpp in the target directory."""
  for filename in os.listdir(dir):
    file_path = os.path.join(dir, filename)
    if file_path.endswith('.hpp'):
      os.remove(file_path)


def format_comment(comment, width=100, indent_width=0):
  """Wrap a comment string into C++ // lines at the given column/indent."""
  words = (comment or "").split()
  line_width = 3 + indent_width
  lines = []
  current_line = []
  indent = ' ' * indent_width
  for word in words:
    if line_width + len(word) + 1 <= width:
      current_line.append(word)
      line_width += len(word) + 1
    else:
      lines.append(indent + "// " + " ".join(current_line))
      current_line = [word]
      line_width = 3 + indent_width + len(word) + 1
  if current_line:
    lines.append(indent + "// " + " ".join(current_line))
  return "\n".join(lines)


# ------------------------------------------------------------------------------------
# SVD attribute → ftl::mmio type mapping

def mmio_access(access):
  """Map SVDAccessType to ftl::mmio access tag."""
  if access == SVDAccessType.READ_ONLY:        return "ftl::mmio::RO"
  if access == SVDAccessType.WRITE_ONLY:       return "ftl::mmio::WO"
  if access == SVDAccessType.WRITE_ONCE:       return "ftl::mmio::WO"
  if access == SVDAccessType.READ_WRITE:       return "ftl::mmio::RW"
  if access == SVDAccessType.READ_WRITE_ONCE:  return "ftl::mmio::RW"
  # Default: treat unspecified as RW (SVD default per spec).
  return "ftl::mmio::RW"


def mmio_modify_write(mwv):
  """Map SVDModifiedWriteValuesType to ftl::mmio modified-write tag."""
  if mwv is None:
    return "ftl::mmio::Normal"
  name = getattr(mwv, 'name', str(mwv))
  table = {
    'ONE_TO_CLEAR':   "ftl::mmio::OneToClear",
    'ONE_TO_SET':     "ftl::mmio::OneToSet",
    'ONE_TO_TOGGLE':  "ftl::mmio::OneToToggle",
    'ZERO_TO_CLEAR':  "ftl::mmio::ZeroToClear",
    'ZERO_TO_SET':    "ftl::mmio::ZeroToSet",
    'ZERO_TO_TOGGLE': "ftl::mmio::ZeroToToggle",
    'CLEAR':          "ftl::mmio::ClearOnWrite",
    'SET':            "ftl::mmio::SetOnWrite",
    'MODIFY':         "ftl::mmio::Normal",
  }
  return table.get(name, "ftl::mmio::Normal")


# ------------------------------------------------------------------------------------
# Register layout helpers

def normalize_register_name(name):
  """'FOO[3]' → 'FOO_3' for array-element registers."""
  if '[' in name and ']' in name:
    base = name.split('[')[0]
    idx = name.split('[')[1].split(']')[0]
    return f'{base}_{idx}'
  return name


def register_size(register):
  """Return the register's bit width (default 32)."""
  return register.size if register.size is not None else 32


def register_storage_type(register):
  """Return the C++ unsigned storage type matching the register's bit width."""
  bits = register_size(register)
  if bits <= 8:  return 'std::uint8_t'
  if bits <= 16: return 'std::uint16_t'
  return 'std::uint32_t'


def register_reset_literal(register):
  """Format a reset-value literal sized to match the register's storage type."""
  bits = register_size(register)
  reset = register.reset_value or 0
  width_hex = {8: 2, 16: 4, 32: 8}.get(bits if bits in (8, 16, 32) else 32, 8)
  return f'0x{reset:0{width_hex}X}u'


def field_value_type(field):
  """
  Pick the C++ type name used for a field's value.

  - If the field has a single enumeratedValues set with a name, use the enum class
    generated from it (prefixed with 'e').
  - Otherwise fall back to a sensible integer type.
  """
  if field.is_enumerated_type and field.enumerated_values and len(field.enumerated_values) == 1:
    es = field.enumerated_values[0]
    enum_name = es.name if es.name else field.name
    return f'e{enum_name}'
  w = field.bit_width
  if w == 1:
    return 'bool'
  if w <= 8:
    return 'std::uint8_t'
  if w <= 16:
    return 'std::uint16_t'
  return 'std::uint32_t'


def build_field_rows(register):
  """
  Produce an ordered list of "rows" covering every bit of the register:
    [ {'kind': 'field',    'name': ..., 'offset': ..., 'width': ..., 'field': SVDField,
       'value_type': 'eMUX_MODE' or 'bool' or ..., 'access': '...', 'modify': '...'},
      {'kind': 'reserved', 'offset': ..., 'width': ...},
      ... ]
  Rows are sorted by bit_offset and gaps between fields are filled with 'reserved' rows.
  """
  reg_bits = register_size(register)
  rows = []
  cursor = 0
  for field in sorted(register.fields, key=lambda f: f.bit_offset):
    if field.bit_offset > cursor:
      rows.append({
        'kind': 'reserved',
        'offset': cursor,
        'width':  field.bit_offset - cursor,
      })
    rows.append({
      'kind': 'field',
      'name': field.name,
      'offset': field.bit_offset,
      'width':  field.bit_width,
      'field':  field,
      'value_type': field_value_type(field),
      'access': mmio_access(field.access),
      'modify': mmio_modify_write(field.modified_write_values),
    })
    cursor = field.bit_offset + field.bit_width
  if cursor < reg_bits:
    rows.append({'kind': 'reserved', 'offset': cursor, 'width': reg_bits - cursor})
  return rows


# =================================================================================================
# CMSIS-SVD parser wrapper

class SVDParserWrapper:
  """
  Generates type-safe peripheral register header files from a CMSIS-SVD XML file.
  Output targets the ftl::mmio primitives: no bitfield unions, policy-checked Register<>.
  """

  def __init__(self, svd_file, output_dir):
    print(f"Using svd file: {svd_file}")
    self.output_dir = output_dir

    # Cache the parsed device on first run; reload from pickle afterwards.
    svd_filename = os.path.splitext(os.path.basename(svd_file))[0]
    cache_path = os.path.join(BIN_DIR, f"{svd_filename}.pkl")

    if os.path.exists(cache_path):
      print(f"Loading device from: {cache_path}")
      with open(cache_path, 'rb') as f:
        self.device = pickle.load(f)
      print("Done loading from cache.")
    else:
      print(f"Parsing and saving device to {cache_path}")
      self.svd_parser = SVDParser.for_xml_file(svd_file)
      self.device = self.svd_parser.get_device()
      os.makedirs(BIN_DIR, exist_ok=True)
      with open(cache_path, 'wb') as f:
        pickle.dump(self.device, f)
      print("Done saving to cache.")

    template_dir = os.path.join(FORGE_ROOT, 'scripts', 'templates')
    self.env = Environment(loader=FileSystemLoader(template_dir))
    self.env.globals.update(
      format_comment=format_comment,
      normalize_register_name=normalize_register_name,
      register_size=register_size,
      register_storage_type=register_storage_type,
      register_reset_literal=register_reset_literal,
      build_field_rows=build_field_rows,
      mmio_access=mmio_access,
      mmio_modify_write=mmio_modify_write,
      SVDAccessType=SVDAccessType,
    )
    self.template = self.env.get_template('cmsis_svd_registers.jinja2')

  def _render(self, peripheral):
    return self.template.render(peripheral=peripheral)

  def _write_peripheral(self, peripheral):
    rendered = self._render(peripheral)
    out_path = os.path.join(self.output_dir, f'{peripheral.name}.hpp'.lower())
    with open(out_path, 'w') as f:
      f.write(rendered)
    return out_path

  def generate_peripheral(self, peripheral_name: str):
    """Generate a single peripheral header. Does NOT wipe sibling headers."""
    all_names = []
    generated = False
    for peripheral in self.device.peripherals:
      all_names.append(peripheral.name)
      if peripheral.name == peripheral_name:
        out = self._write_peripheral(peripheral)
        print(f"Generated: {out}")
        generated = True
    if not generated:
      print(f"Could not find peripheral, possible options include:\n{all_names}")

  def generate(self):
    """Regenerate all peripheral headers (wipes the output directory first)."""
    clean_headers(self.output_dir)
    for peripheral in self.device.peripherals:
      self._write_peripheral(peripheral)
    print(f"Register definitions generated in: {self.output_dir}")
