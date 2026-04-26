"""Model objects wrapping cmsis-svd parser output.

Each class wraps one SVD concept and exposes the data the templates need.
Methods that build new objects are spelled with parens (`peripheral.registers()`,
`register.fields()`); cheap derivations are properties. The conversion logic
lives on the classes themselves — there are no separate "build_*" functions.

Public entry points:
    standalone_peripheral(svd_peripheral)        -> Peripheral
    family(canonical_name, members)              -> (PeripheralFamily | None,
                                                      orphan_members)
"""

import re
from dataclasses import dataclass
from typing import Optional, Union

from cmsis_svd.model import (
    SVDAccessType,
    SVDRegister,
    SVDRegisterArray,
    SVDRegisterClusterArray,
)


# =================================================================================================
# Public entry points

def standalone_peripheral(svd_peripheral):
  """Wrap a standalone (non-family) SVD peripheral. Addresses are absolute."""
  return Peripheral(svd_peripheral, _AddrMode.absolute(svd_peripheral.base_address))


def family(canonical_name, members):
  """Group derivedFrom siblings into a PeripheralFamily.

  Returns (family_or_None, orphan_members). family is None and the caller
  should treat each member as standalone if there aren't at least 2
  numbered instances. orphan_members are members of the group that don't
  fit the digit-suffixed naming pattern.
  """
  family_name = re.sub(r"\d+$", "", canonical_name) or canonical_name
  instances = []
  orphans = []
  for m in members:
    suffix = m.name[len(family_name):]
    if suffix.isdigit():
      instances.append(FamilyInstance(int(suffix), m.name, m.base_address))
    else:
      orphans.append(m)
  if len(instances) < 2:
    return None, members
  instances.sort(key=lambda i: i.number)
  parent = next((p for p in members if p.name == canonical_name), members[0])
  return PeripheralFamily(parent, family_name, instances), orphans


# =================================================================================================
# Peripheral / PeripheralFamily

class Peripheral:
  """One peripheral worth of registers. Wraps an SVDPeripheral plus the
  address-emission mode (absolute for standalone, kBase-symbolic for family)."""

  def __init__(self, svd_peripheral, addr_mode):
    self._svd = svd_peripheral
    self._addr = addr_mode

  @property
  def name(self):
    return self._svd.name

  @property
  def description(self):
    return self._svd.description

  @property
  def lower_name(self):
    return self.name.lower()

  @property
  def type_qualifier(self):
    return self._addr.type_qualifier

  def registers(self):
    """Yield Register objects in declaration order. Cluster arrays expand
    into one Register per inner register."""
    for r in self._svd.registers:
      yield from Register.expand(r, self._svd, self._addr)


@dataclass(frozen=True)
class FamilyInstance:
  number: int
  name: str
  base_address: int


class PeripheralFamily:
  """A peripheral family (derivedFrom siblings sharing one register layout).
  The parent peripheral renders inside a templated struct; instances supply
  kBase per Instance."""

  def __init__(self, svd_parent, family_name, instances):
    self._parent = Peripheral(svd_parent, _AddrMode.symbolic("kBase"))
    self._family_name = family_name
    self._instances = instances

  @property
  def family_name(self):
    return self._family_name

  @property
  def class_name(self):
    return self._family_name.capitalize()

  @property
  def lower_name(self):
    return self._family_name.lower()

  @property
  def instances(self):
    return self._instances

  @property
  def parent(self):
    return self._parent

  def cpp_instance_assert(self):
    clauses = " || ".join(f"Instance == {i.number}u" for i in self._instances)
    nums = ", ".join(str(i.number) for i in self._instances)
    return (f'static_assert(\n      {clauses},\n'
            f'      "{self.class_name}: Instance must be one of {nums}");')


# =================================================================================================
# Register

class Register:
  """One register declaration. Multiple Registers can come from one SVD entry
  when expanding cluster arrays."""

  @classmethod
  def expand(cls, svd, svd_peripheral, addr_mode):
    """Yield 1+ Register objects from one SVD register/array/cluster-array."""
    if isinstance(svd, SVDRegister):
      yield cls._plain(svd, svd_peripheral, addr_mode)
    elif isinstance(svd, SVDRegisterArray):
      yield cls._array(svd, svd_peripheral, addr_mode)
    elif isinstance(svd, SVDRegisterClusterArray):
      yield from cls._cluster_array(svd, svd_peripheral, addr_mode)
    else:
      raise NotImplementedError(
          f"Peripheral {svd_peripheral.name!r} has a register of type "
          f"{type(svd).__name__} which the generator does not yet emit.")

  @classmethod
  def _plain(cls, svd, svd_peripheral, addr_mode):
    return cls(svd,
               name=_normalize_name(svd.name),
               offset=svd.address_offset,
               template_params=[],
               addr_mode=addr_mode,
               peripheral_name=svd_peripheral.name)

  @classmethod
  def _array(cls, svd_array, svd_peripheral, addr_mode):
    proto = svd_array.registers[0]
    meta = svd_array.meta_register
    return cls(proto,
               name=_normalize_name(_strip_dim_placeholder(meta.name)),
               offset=proto.address_offset,
               template_params=[TemplateParam(
                   "Index", len(svd_array.registers), meta.dim_increment)],
               addr_mode=addr_mode,
               peripheral_name=svd_peripheral.name)

  @classmethod
  def _cluster_array(cls, svd_cluster_array, svd_peripheral, addr_mode):
    cluster_proto = svd_cluster_array.clusters[0]
    cluster_param = TemplateParam(
        "ClusterIndex", len(svd_cluster_array.clusters), cluster_proto.dim_increment)
    prefix = cluster_proto.name + "_"

    def strip_prefix(name):
      return name[len(prefix):] if name.startswith(prefix) else name

    for inner in cluster_proto.registers:
      where = f"{svd_peripheral.name}.{cluster_proto.name}"
      if isinstance(inner, SVDRegister):
        offset = _resolve_inner_offset(
            inner.address_offset, cluster_proto, f"{where}.{inner.name}")
        yield cls(inner,
                  name=_normalize_name(strip_prefix(inner.name)),
                  offset=offset,
                  template_params=[cluster_param],
                  addr_mode=addr_mode,
                  peripheral_name=svd_peripheral.name)
      elif isinstance(inner, SVDRegisterArray):
        proto = inner.registers[0]
        meta = inner.meta_register
        offset = _resolve_inner_offset(
            proto.address_offset, cluster_proto, f"{where}.{meta.name}")
        yield cls(proto,
                  name=_normalize_name(strip_prefix(_strip_dim_placeholder(meta.name))),
                  offset=offset,
                  template_params=[cluster_param, TemplateParam(
                      "ArrayIndex", len(inner.registers), meta.dim_increment)],
                  addr_mode=addr_mode,
                  peripheral_name=svd_peripheral.name)
      else:
        raise NotImplementedError(
            f"Peripheral {svd_peripheral.name!r} cluster {cluster_proto.name!r} "
            f"contains an inner element of type {type(inner).__name__} which is "
            f"not yet supported.")

  def __init__(self, svd_register, *,
               name, offset, template_params, addr_mode, peripheral_name):
    self._svd = svd_register
    self._name = name
    self._offset = offset
    self._template_params = template_params
    self._addr_mode = addr_mode
    self._peripheral_name = peripheral_name
    self._validate_field_names()

  @property
  def name(self):
    return self._name

  @property
  def description(self):
    return self._svd.description

  @property
  def addr_expr(self):
    return self._addr_mode.format(self._offset, self._template_params)

  @property
  def storage_type(self):
    return _storage_type(self._bits)

  @property
  def reset_literal(self):
    return _reset_literal(self._bits, self._svd.reset_value or 0)

  @property
  def access(self):
    return _access_to_mmio(self._svd.access)

  @property
  def template_params(self):
    return self._template_params

  @property
  def is_templated(self):
    return bool(self._template_params)

  def cpp_template_decl(self):
    if not self._template_params:
      return ""
    params = ", ".join(p.cpp_decl() for p in self._template_params)
    return f"template<{params}>"

  def fields(self):
    return [Field(f) for f in sorted(self._svd.fields, key=lambda f: f.bit_offset)]

  def slots(self):
    """Field/Reserved gap-filled list covering every bit of the register."""
    out = []
    cursor = 0
    for f in self.fields():
      if f.offset > cursor:
        out.append(Reserved(cursor, f.offset - cursor))
      out.append(f)
      cursor = f.offset + f.width
    if cursor < self._bits:
      out.append(Reserved(cursor, self._bits - cursor))
    return out

  def enums(self):
    """Distinct enums declared by this register's fields, in field order."""
    seen = set()
    out = []
    for f in self.fields():
      for e in f.enums():
        if e.name not in seen:
          seen.add(e.name)
          out.append(e)
    return out

  @property
  def _bits(self):
    return self._svd.size if self._svd.size is not None else 32

  def _validate_field_names(self):
    """cpp_reexport renames a field whose name matches the register to VALUE.
    Fail loud if that rename target is already taken (or if the register
    itself is named VALUE) — silent collision would compile but pick the
    wrong type."""
    field_names = [f.name for f in self._svd.fields]
    if self._name not in field_names:
      return
    if self._name == _REEXPORT_FALLBACK:
      raise ValueError(
          f"{self._peripheral_name}.{self._name}: cannot apply rename fallback — "
          f"the register itself is named {_REEXPORT_FALLBACK!r} so the renamed "
          f"field would still shadow the enclosing struct. Edit the SVD or "
          f"change the rename strategy in Field.cpp_reexport.")
    if _REEXPORT_FALLBACK in field_names:
      raise ValueError(
          f"{self._peripheral_name}.{self._name}: rename fallback collision — "
          f"field {self._name!r} shares the register name and would be "
          f"re-exported as {_REEXPORT_FALLBACK!r}, but a field named "
          f"{_REEXPORT_FALLBACK!r} already exists in this register. Edit the "
          f"SVD or change the rename strategy in Field.cpp_reexport.")


# =================================================================================================
# Field / Reserved / Enum

class Field:
  """One bit-field within a register. Wraps an SVDField."""

  def __init__(self, svd_field):
    self._svd = svd_field

  @property
  def name(self):
    return self._svd.name

  @property
  def description(self):
    return self._svd.description

  @property
  def offset(self):
    return self._svd.bit_offset

  @property
  def width(self):
    return self._svd.bit_width

  @property
  def value_type(self):
    """C++ type used for the field's value: enum class for enumerated fields,
    else the smallest unsigned int that fits the width (or bool for w==1)."""
    enums = self.enums()
    if len(enums) == 1:
      return f"e{enums[0].name}"
    w = self.width
    if w == 1:  return "bool"
    if w <= 8:  return "std::uint8_t"
    if w <= 16: return "std::uint16_t"
    return "std::uint32_t"

  @property
  def access(self):
    return _access_to_mmio(self._svd.access)

  @property
  def modify(self):
    return _modify_write_to_mmio(self._svd.modified_write_values, where=self.name)

  def enums(self):
    if not (self._svd.is_enumerated_type and self._svd.enumerated_values):
      return []
    return [Enum(es, self.name) for es in self._svd.enumerated_values]

  def cpp_using(self):
    return (f"using {self.name} = ftl::mmio::Field<"
            f"{self.width}, {self.offset}, {self.value_type}, "
            f"{self.access}, {self.modify}>;")

  def cpp_template_arg(self, register_name, type_qualifier=""):
    return f"{type_qualifier}{register_name}_fields_::{self.name}"

  def cpp_reexport(self, register_name, type_qualifier=""):
    # When a field shares its register's name (e.g. GPIO::DR), re-exporting
    # it under that name shadows the enclosing struct's injected-class-name.
    # Emit it as VALUE instead. Register._validate_field_names guarantees the
    # rename target is unique within the register.
    rhs = f"{type_qualifier}{register_name}_fields_::{self.name}"
    lhs = _REEXPORT_FALLBACK if self.name == register_name else self.name
    return f"using {lhs} = {rhs};"


@dataclass(frozen=True)
class Reserved:
  offset: int
  width: int

  # register_name and type_qualifier are unused but required so this method
  # is polymorphic with Field.cpp_template_arg — jinja calls them uniformly.
  def cpp_template_arg(self, register_name="", type_qualifier=""):
    return f"ftl::mmio::Reserved<{self.width}, {self.offset}>"


class Enum:
  """One enumeratedValues set, generated as a C++ `enum class`."""

  def __init__(self, svd_enum_set, fallback_name):
    self._svd = svd_enum_set
    self._fallback = fallback_name

  @property
  def name(self):
    return self._svd.name or self._fallback

  def values(self):
    return [EnumValue(v) for v in self._svd.enumerated_values]


class EnumValue:
  def __init__(self, svd_enum_value):
    self._svd = svd_enum_value

  @property
  def name(self):
    return self._svd.name

  @property
  def value(self):
    return self._svd.value

  @property
  def description(self):
    return self._svd.description


@dataclass(frozen=True)
class TemplateParam:
  name: str    # 'Index' | 'ClusterIndex' | 'ArrayIndex'
  dim: int
  increment: int

  def cpp_decl(self):
    return f"std::uint32_t {self.name}"

  def cpp_static_assert(self, register_name):
    return (f'static_assert({self.name} < {self.dim}u, '
            f'"{register_name}: {self.name} out of range");')


# =================================================================================================
# Address mode (encapsulates standalone-vs-family addressing for one peripheral)

@dataclass(frozen=True)
class _AddrMode:
  base_address: Optional[int]
  base_symbol: Optional[str]
  type_qualifier: str

  @classmethod
  def absolute(cls, base_address):
    return cls(base_address=base_address, base_symbol=None, type_qualifier="")

  @classmethod
  def symbolic(cls, symbol):
    return cls(base_address=None, base_symbol=symbol, type_qualifier="typename ")

  def format(self, offset, template_params):
    if self.base_symbol is None:
      expr = f"0x{self.base_address + offset:08X}u"
    else:
      expr = f"{self.base_symbol} + 0x{offset:X}u"
    for tp in template_params:
      expr += f" + ({tp.name} * 0x{tp.increment:X}u)"
    return expr


# =================================================================================================
# Cluster-inner offset resolution

def _resolve_inner_offset(inner_offset, cluster, where):
  """Resolve a cluster-inner register's offset to a peripheral-relative offset.

  SVDs disagree on whether an inner <addressOffset> is cluster-relative (per
  spec) or peripheral-relative (NXP MIMXRT DMA TCD style). cmsis-svd always
  treats it as cluster-relative and adds the cluster offset, double-counting
  the broken case. Try both interpretations against the cluster's stride: use
  whichever uniquely fits, raise loudly when both fit (ambiguous) or neither
  fits (malformed).
  """
  spec = inner_offset
  fix = inner_offset - cluster.address_offset
  span_start = cluster.address_offset
  span_end = cluster.address_offset + cluster.dim_increment
  spec_ok = span_start <= spec < span_end
  fix_ok = span_start <= fix < span_end

  if spec_ok and not fix_ok:
    return spec
  if fix_ok and not spec_ok:
    return fix
  if spec_ok and fix_ok:
    if spec == fix:
      return spec   # cluster.address_offset == 0 → both interpretations agree
    raise ValueError(
        f"{where}: ambiguous cluster-inner offset {inner_offset:#x}. "
        f"Both spec-conformant ({spec:#x}) and double-counted ({fix:#x}) "
        f"interpretations land within the cluster's "
        f"[{span_start:#x}, {span_end:#x}) stride. Fix the SVD so only one "
        f"interpretation is valid.")
  raise ValueError(
      f"{where}: unresolvable cluster-inner offset. cmsis-svd resolved "
      f"inner_offset={inner_offset:#x}; neither spec-conformant ({spec:#x}) "
      f"nor double-counted ({fix:#x}) lands within the cluster's "
      f"[{span_start:#x}, {span_end:#x}) stride.")


# =================================================================================================
# SVD attribute → C++ / ftl::mmio mappings

_REEXPORT_FALLBACK = "VALUE"


_ACCESS_TO_MMIO = {
    SVDAccessType.READ_ONLY:       "ftl::mmio::RO",
    SVDAccessType.WRITE_ONLY:      "ftl::mmio::WO",
    SVDAccessType.WRITE_ONCE:      "ftl::mmio::WO",
    SVDAccessType.READ_WRITE:      "ftl::mmio::RW",
    SVDAccessType.READ_WRITE_ONCE: "ftl::mmio::RW",
}


def _access_to_mmio(access):
  return _ACCESS_TO_MMIO.get(access, "ftl::mmio::RW")


_MODIFY_WRITE_TO_MMIO = {
    "ONE_TO_CLEAR":  "ftl::mmio::OneToClear",
    "ONE_TO_SET":    "ftl::mmio::OneToSet",
    "ONE_TO_TOGGLE": "ftl::mmio::OneToToggle",
    "MODIFY":        "ftl::mmio::Normal",
}


def _modify_write_to_mmio(mwv, *, where):
  if mwv is None:
    return "ftl::mmio::Normal"
  name = getattr(mwv, "name", str(mwv))
  if name in _MODIFY_WRITE_TO_MMIO:
    return _MODIFY_WRITE_TO_MMIO[name]
  raise NotImplementedError(
      f"<modifiedWriteValues>{name.lower()}</modifiedWriteValues> on {where} "
      f"is not supported by ftl::mmio. "
      f"Supported values: {sorted(_MODIFY_WRITE_TO_MMIO)}.")


def _storage_type(bits):
  if bits <= 8:  return "std::uint8_t"
  if bits <= 16: return "std::uint16_t"
  return "std::uint32_t"


def _reset_literal(bits, reset):
  digits = {8: 2, 16: 4}.get(bits, 8)
  return f"0x{reset:0{digits}X}u"


def _normalize_name(name):
  """'FOO[3]' → 'FOO_3' for array-element registers."""
  m = re.fullmatch(r"(.+)\[(\d+)\]", name)
  return f"{m.group(1)}_{m.group(2)}" if m else name


def _strip_dim_placeholder(name):
  """Remove the [%s] / %s SVD <dim> placeholder from an array name."""
  return name.replace("[%s]", "").replace("%s", "")
