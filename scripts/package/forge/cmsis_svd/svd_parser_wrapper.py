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


_SUPPORTED_MODIFY_WRITE = {
  'ONE_TO_CLEAR':   "ftl::mmio::OneToClear",
  'ONE_TO_SET':     "ftl::mmio::OneToSet",
  'ONE_TO_TOGGLE':  "ftl::mmio::OneToToggle",
  'MODIFY':         "ftl::mmio::Normal",
}


def mmio_modify_write(mwv, *, field_name=None, register_name=None):
  """Map SVDModifiedWriteValuesType to ftl::mmio modified-write tag.

  Fails loud on values ftl::mmio::Register::modify() can't safely handle yet
  (ZERO_TO_*, CLEAR, SET) so we don't silently emit headers that misbehave on
  RMW. To support a new value: extend ftl::mmio + add a fixture in
  forge/test/native/mmio/test_peripheral.svd before regenerating.
  """
  if mwv is None:
    return "ftl::mmio::Normal"
  name = getattr(mwv, 'name', str(mwv))
  if name not in _SUPPORTED_MODIFY_WRITE:
    where = ''
    if register_name and field_name:
      where = f' (register {register_name}, field {field_name})'
    raise NotImplementedError(
        f"<modifiedWriteValues>{name.lower()}</modifiedWriteValues>"
        f"{where} is not supported by ftl::mmio. "
        f"Supported values: {sorted(_SUPPORTED_MODIFY_WRITE)}.")
  return _SUPPORTED_MODIFY_WRITE[name]


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


def _undo_double_counted_offset(inner_offset, container_offset, container_dim_increment):
  """Some vendor SVDs (e.g. NXP MIMXRT DMA TCDs) put peripheral-relative
  offsets on inner registers instead of container-relative. cmsis-svd then
  uniformly adds the container offset, double-counting. Detect when the
  resolved offset lands past the container's own range and undo it."""
  if inner_offset >= container_offset + container_dim_increment:
    return inner_offset - container_offset
  return inner_offset


def _format_addr_expr(base_expr, offset, template_params):
  """Build the address expression substituted into the Register<> template
  parameter list. Two emission modes:
    base_expr is None  -> '0x{absolute:08X}u'                  (standalone peripheral)
    base_expr is a str -> '{base_expr} + 0x{offset:X}u'        (peripheral family;
                                                                base resolved by the
                                                                enclosing struct's kBase)
  Plus optional ' + (P0 * inc0) + (P1 * inc1) + ...' tail for cluster/array indices.
  """
  if base_expr is None:
    expr = f'0x{offset:08X}u'
  else:
    expr = f'{base_expr} + 0x{offset:X}u'
  for tp in template_params:
    expr += f' + ({tp["name"]} * 0x{tp["increment"]:X}u)'
  return expr


def _make_render_item(register, name, *, abs_addr=None, offset=None,
                      base_expr=None, template_params):
  """Uniform render-item shape regardless of how the register is reached.

  Pass either abs_addr (absolute address, base_expr must be None) or offset
  (peripheral-relative offset, base_expr must name the enclosing kBase symbol).
  template_params is a list of dicts {'name', 'dim', 'increment'}; empty = plain
  register, length 1 = 1D array/cluster, length 2 = nested cluster-of-array."""
  if base_expr is None:
    assert abs_addr is not None and offset is None
    addr_offset = abs_addr
  else:
    assert offset is not None and abs_addr is None
    addr_offset = offset
  return {
    'register': register,
    'name': normalize_register_name(name),
    'addr_expr': _format_addr_expr(base_expr, addr_offset, template_params),
    'is_templated': bool(template_params),
    'template_params': template_params,
  }


def peripheral_render_items(peripheral, *, base_expr=None):
  """Flatten a peripheral's registers + arrays + cluster arrays into a uniform
  iterable of render-item dicts that the jinja template can render without
  needing to know about container shape.

  base_expr controls address emission:
    None           -> absolute addresses (standalone peripheral, namespace shape)
    'kBase'        -> '{base_expr} + offset' (peripheral family, templated class
                       shape — the enclosing struct supplies kBase per Instance)

  Supports:
    SVDRegister                                  -> 0 template params
    SVDRegisterArray                             -> 1 template param (Index)
    SVDRegisterClusterArray of SVDRegister       -> 1 (ClusterIndex)
    SVDRegisterClusterArray of SVDRegisterArray  -> 2 (ClusterIndex, ArrayIndex)
  """
  # In family mode the per-register address is peripheral-relative (kBase
  # supplies the rest); in standalone mode it's absolute.
  def _addr_args(register_offset):
    if base_expr is None:
      return {'abs_addr': peripheral.base_address + register_offset}
    return {'offset': register_offset}

  for r in peripheral.registers:
    cls = r.__class__.__name__
    if cls == 'SVDRegister':
      yield _make_render_item(
          r,
          name=r.name,
          base_expr=base_expr,
          template_params=[],
          **_addr_args(r.address_offset))
    elif cls == 'SVDRegisterArray':
      # A single register replicated by <dim> (e.g. DMAMUX CHCFG[N]).
      proto = r.registers[0]
      meta = r.meta_register
      base_name = meta.name.replace('[%s]', '').replace('%s', '')
      yield _make_render_item(
          proto,
          name=base_name,
          base_expr=base_expr,
          template_params=[{
            'name': 'Index',
            'dim': len(r.registers),
            'increment': meta.dim_increment,
          }],
          **_addr_args(proto.address_offset))
    elif cls == 'SVDRegisterClusterArray':
      # An array of multi-register clusters (e.g. DMA TCD[0..31]).
      cluster_proto = r.clusters[0]
      cluster_param = {
        'name': 'ClusterIndex',
        'dim': len(r.clusters),
        'increment': cluster_proto.dim_increment,
      }
      cluster_prefix = cluster_proto.name + '_'
      def _strip(name):
        return name[len(cluster_prefix):] if name.startswith(cluster_prefix) else name
      for inner in cluster_proto.registers:
        inner_cls = inner.__class__.__name__
        if inner_cls == 'SVDRegister':
          inner_offset = _undo_double_counted_offset(
              inner.address_offset,
              cluster_proto.address_offset,
              cluster_proto.dim_increment)
          yield _make_render_item(
              inner,
              name=_strip(inner.name),
              base_expr=base_expr,
              template_params=[cluster_param],
              **_addr_args(inner_offset))
        elif inner_cls == 'SVDRegisterArray':
          # A register-array nested inside each cluster instance — e.g. CCM
          # CLOCK_ROOT[C]_CLOCK_ROOT_SETPOINT[S]. Two template params: cluster
          # index and array index.
          array_proto = inner.registers[0]
          array_meta = inner.meta_register
          array_offset = _undo_double_counted_offset(
              array_proto.address_offset,
              cluster_proto.address_offset,
              cluster_proto.dim_increment)
          array_base = array_meta.name.replace('[%s]', '').replace('%s', '')
          yield _make_render_item(
              array_proto,
              name=_strip(array_base),
              base_expr=base_expr,
              template_params=[cluster_param, {
                'name': 'ArrayIndex',
                'dim': len(inner.registers),
                'increment': array_meta.dim_increment,
              }],
              **_addr_args(array_offset))
        else:
          raise NotImplementedError(
              f"Peripheral {peripheral.name!r} cluster {cluster_proto.name!r} "
              f"contains an inner element of type {inner_cls} which is not "
              f"yet supported. Extend peripheral_render_items() and add a "
              f"fixture in forge/test/native/mmio/test_peripheral.svd.")
    else:
      raise NotImplementedError(
          f"Peripheral {peripheral.name!r} has a register of type {cls} "
          f"which the generator does not yet support.")


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
      'modify': mmio_modify_write(
          field.modified_write_values,
          field_name=field.name,
          register_name=register.name),
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

    # Cache the parsed device. Invalidate the pickle when the SVD source is
    # newer than the cache — otherwise edits to test fixtures (or vendor SVDs)
    # silently use a stale parse.
    svd_filename = os.path.splitext(os.path.basename(svd_file))[0]
    cache_path = os.path.join(BIN_DIR, f"{svd_filename}.pkl")

    cache_fresh = (
        os.path.exists(cache_path)
        and os.path.getmtime(cache_path) >= os.path.getmtime(svd_file))

    if cache_fresh:
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
      peripheral_render_items=peripheral_render_items,
      SVDAccessType=SVDAccessType,
    )
    self.standalone_template = self.env.get_template('cmsis_svd_registers.jinja2')
    self.family_template = self.env.get_template('cmsis_svd_family.jinja2')

  _SUPPORTED_REGISTER_CLASSES = {
    'SVDRegister',
    'SVDRegisterArray',
    'SVDRegisterClusterArray',
  }

  def _validate_peripheral(self, peripheral):
    for register in peripheral.registers:
      cls = register.__class__.__name__
      if cls not in self._SUPPORTED_REGISTER_CLASSES:
        name = getattr(register, 'name', '<unnamed>')
        raise NotImplementedError(
            f"Peripheral {peripheral.name!r} contains a register {name!r} "
            f"of type {cls} which the generator does not yet emit. "
            f"Add support to peripheral_render_items() + the jinja templates, "
            f"plus a fixture in forge/test/native/mmio/test_peripheral.svd, "
            f"before regenerating.")

  @staticmethod
  def _strip_trailing_digits(name):
    i = len(name)
    while i > 0 and name[i - 1].isdigit():
      i -= 1
    return name[:i]

  def _group_peripherals(self):
    """Walk peripherals, identify peripheral families (SVD <derivedFrom>
    siblings sharing a register layout) and standalone peripherals.

    Returns (standalone_peripherals, families). Each family is a dict with
    keys: parent (canonical SVDPeripheral), family_name, class_name, instances
    (sorted list of (instance_number, SVDPeripheral)).
    """
    by_canonical = {}  # parent name -> [members]
    for p in self.device.peripherals:
      canonical = p.derived_from or p.name
      by_canonical.setdefault(canonical, []).append(p)

    standalone = []
    families = []
    for canonical_name, members in by_canonical.items():
      if len(members) < 2:
        standalone.append(members[0])
        continue
      family_name = self._strip_trailing_digits(canonical_name) or canonical_name
      parent = next((p for p in members if p.name == canonical_name), members[0])
      instances = []
      orphans = []
      for m in members:
        suffix = m.name[len(family_name):]
        if suffix.isdigit():
          instances.append((int(suffix), m))
        else:
          orphans.append(m)
      if len(instances) < 2:
        # No usable digit-suffixed instances; treat all members as standalone.
        standalone.extend(members)
        continue
      standalone.extend(orphans)
      # Sort by instance number only (peripherals are not orderable).
      instances.sort(key=lambda kv: kv[0])
      families.append({
        'parent': parent,
        'family_name': family_name,
        'class_name': family_name.capitalize(),
        'instances': instances,
      })
    return standalone, families

  def _render_standalone(self, peripheral):
    self._validate_peripheral(peripheral)
    return self.standalone_template.render(peripheral=peripheral)

  def _render_family(self, family):
    self._validate_peripheral(family['parent'])
    return self.family_template.render(family=family, peripheral=family['parent'])

  def _write_standalone(self, peripheral):
    rendered = self._render_standalone(peripheral)
    out_path = os.path.join(self.output_dir, f'{peripheral.name}.hpp'.lower())
    with open(out_path, 'w') as f:
      f.write(rendered)
    return out_path

  def _write_family(self, family):
    rendered = self._render_family(family)
    out_path = os.path.join(self.output_dir, f'{family["family_name"]}.hpp'.lower())
    with open(out_path, 'w') as f:
      f.write(rendered)
    return out_path

  def generate_peripheral(self, peripheral_name: str):
    """Generate a single peripheral or family header. If peripheral_name is a
    member of a family (siblings via derivedFrom), the entire family file is
    regenerated. Other peripherals' headers are left alone."""
    standalone, families = self._group_peripherals()
    for fam in families:
      if any(m.name == peripheral_name for _, m in fam['instances']):
        out = self._write_family(fam)
        print(f"Generated family: {out}")
        return
    for p in standalone:
      if p.name == peripheral_name:
        out = self._write_standalone(p)
        print(f"Generated: {out}")
        return
    all_names = [p.name for p in self.device.peripherals]
    print(f"Could not find peripheral, possible options include:\n{all_names}")

  def generate(self):
    """Regenerate all peripheral headers (wipes the output directory first)."""
    clean_headers(self.output_dir)
    standalone, families = self._group_peripherals()
    for p in standalone:
      self._write_standalone(p)
    for fam in families:
      self._write_family(fam)
    print(f"Register definitions generated in: {self.output_dir}")
