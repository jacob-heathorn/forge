"""Model classes used by the jinja templates.

The wrapper builds these from cmsis-svd's parser objects, then renders. Each
class models one concept; templates walk the model directly via attribute and
method access. Builders below convert from cmsis-svd to the model — that's the
only place the SVD shape leaks through.
"""

import re
from dataclasses import dataclass, field as _field
from typing import Optional, Union

from cmsis_svd.model import (
    SVDAccessType,
    SVDRegister,
    SVDRegisterArray,
    SVDRegisterClusterArray,
)


# =================================================================================================
# Public API: build a Peripheral or PeripheralFamily from cmsis-svd objects.

def build_peripheral(svd_peripheral, *, type_qualifier="", base_expr=None):
  """Build a Peripheral from cmsis-svd's SVDPeripheral.

  base_expr controls per-register address emission:
    None      -> absolute hex literals (standalone peripheral)
    'kBase'   -> '{base_expr} + offset'  (peripheral family — kBase per Instance)

  type_qualifier ("" or "typename ") prefixes dependent-name field references
  in family mode where the enclosing struct is a template.
  """
  registers = list(_iter_registers(svd_peripheral, base_expr, type_qualifier))
  return Peripheral(
      name=svd_peripheral.name,
      description=svd_peripheral.description,
      registers=registers,
      type_qualifier=type_qualifier)


def build_family(canonical_name, members):
  """Build a PeripheralFamily from a canonical SVDPeripheral and its
  derivedFrom siblings. Returns None if there aren't at least 2 numbered
  instances (caller falls back to treating each member as standalone)."""
  family_name = re.sub(r"\d+$", "", canonical_name) or canonical_name
  instances = []
  orphans = []
  for m in members:
    suffix = m.name[len(family_name):]
    if suffix.isdigit():
      instances.append(FamilyInstance(
          number=int(suffix), name=m.name, base_address=m.base_address))
    else:
      orphans.append(m)
  if len(instances) < 2:
    return None, members
  instances.sort(key=lambda i: i.number)
  parent = next((p for p in members if p.name == canonical_name), members[0])
  family = PeripheralFamily(
      family_name=family_name,
      class_name=family_name.capitalize(),
      instances=instances,
      parent=build_peripheral(parent, type_qualifier="typename ", base_expr="kBase"))
  return family, orphans


# =================================================================================================
# Model classes — pure model walked by jinja.

@dataclass(frozen=True)
class EnumValue:
  name: str
  value: int
  description: Optional[str]


@dataclass(frozen=True)
class Enum:
  name: str
  values: list  # list[EnumValue]


@dataclass(frozen=True)
class Field:
  name: str
  description: Optional[str]
  offset: int
  width: int
  value_type: str   # already mapped: 'eMUX_MODE' | 'bool' | 'std::uint8_t' | ...
  access: str       # already mapped: 'ftl::mmio::RO' | ...
  modify: str       # already mapped: 'ftl::mmio::OneToClear' | ...
  enums: list = _field(default_factory=list)  # list[Enum]

  def cpp_using(self):
    return (f"using {self.name} = ftl::mmio::Field<"
            f"{self.width}, {self.offset}, {self.value_type}, "
            f"{self.access}, {self.modify}>;")

  def cpp_template_arg(self, register_name, type_qualifier=""):
    return f"{type_qualifier}{register_name}_fields_::{self.name}"

  def cpp_reexport(self, register_name, type_qualifier=""):
    # When a field shares its register's name (e.g. GPIO::DR), re-exporting
    # it under that name shadows the enclosing struct's injected-class-name.
    # Emit it as VALUE instead. _validate_register_field_names() guarantees
    # this rename target is unique within the register.
    rhs = f"{type_qualifier}{register_name}_fields_::{self.name}"
    lhs = "VALUE" if self.name == register_name else self.name
    return f"using {lhs} = {rhs};"


@dataclass(frozen=True)
class Reserved:
  offset: int
  width: int

  # register_name and type_qualifier are unused but required so this method
  # is polymorphic with Field.cpp_template_arg — jinja calls them uniformly.
  def cpp_template_arg(self, register_name="", type_qualifier=""):
    return f"ftl::mmio::Reserved<{self.width}, {self.offset}>"


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


@dataclass(frozen=True)
class Register:
  name: str
  description: Optional[str]
  storage_type: str
  reset_literal: str
  access: str
  addr_expr: str
  template_params: list   # list[TemplateParam]
  fields: list            # list[Field]
  slots: list             # list[Field | Reserved] — ordered, gap-filled

  @property
  def is_templated(self):
    return bool(self.template_params)

  def cpp_template_decl(self):
    if not self.template_params:
      return ""
    params = ", ".join(p.cpp_decl() for p in self.template_params)
    return f"template<{params}>"

  def enums(self):
    """Distinct enums declared by this register's fields, in field order."""
    seen = set()
    out = []
    for f in self.fields:
      for e in f.enums:
        if e.name not in seen:
          seen.add(e.name)
          out.append(e)
    return out


@dataclass(frozen=True)
class FamilyInstance:
  number: int
  name: str
  base_address: int


@dataclass(frozen=True)
class Peripheral:
  name: str
  description: Optional[str]
  registers: list  # list[Register]
  type_qualifier: str = ""

  @property
  def lower_name(self):
    return self.name.lower()


@dataclass(frozen=True)
class PeripheralFamily:
  family_name: str   # "GPIO"
  class_name: str    # "Gpio"
  instances: list    # list[FamilyInstance]
  parent: Peripheral

  @property
  def lower_name(self):
    return self.family_name.lower()

  def cpp_instance_assert(self):
    clauses = " || ".join(f"Instance == {i.number}u" for i in self.instances)
    nums = ", ".join(str(i.number) for i in self.instances)
    return (f'static_assert(\n      {clauses},\n'
            f'      "{self.class_name}: Instance must be one of {nums}");')


# =================================================================================================
# cmsis-svd → model builders

def _iter_registers(svd_peripheral, base_expr, type_qualifier):
  for r in svd_peripheral.registers:
    if isinstance(r, SVDRegister):
      yield _build_plain_register(r, svd_peripheral, base_expr, type_qualifier)
    elif isinstance(r, SVDRegisterArray):
      yield _build_register_array(r, svd_peripheral, base_expr, type_qualifier)
    elif isinstance(r, SVDRegisterClusterArray):
      yield from _build_cluster_array(r, svd_peripheral, base_expr, type_qualifier)
    else:
      raise NotImplementedError(
          f"Peripheral {svd_peripheral.name!r} has a register of type "
          f"{type(r).__name__} which the generator does not yet emit.")


def _build_plain_register(svd, peripheral, base_expr, type_qualifier):
  return _build_register(
      svd, peripheral, base_expr, type_qualifier,
      name=_normalize_name(svd.name), template_params=[])


def _build_register_array(svd_array, peripheral, base_expr, type_qualifier):
  proto = svd_array.registers[0]
  meta = svd_array.meta_register
  tparam = TemplateParam(
      name="Index", dim=len(svd_array.registers), increment=meta.dim_increment)
  return _build_register(
      proto, peripheral, base_expr, type_qualifier,
      name=_normalize_name(_strip_dim_placeholder(meta.name)),
      template_params=[tparam],
      offset_override=proto.address_offset)


def _build_cluster_array(svd_cluster_array, peripheral, base_expr, type_qualifier):
  cluster_proto = svd_cluster_array.clusters[0]
  cluster_param = TemplateParam(
      name="ClusterIndex",
      dim=len(svd_cluster_array.clusters),
      increment=cluster_proto.dim_increment)
  prefix = cluster_proto.name + "_"

  def strip_prefix(name):
    return name[len(prefix):] if name.startswith(prefix) else name

  for inner in cluster_proto.registers:
    where = f"{peripheral.name}.{cluster_proto.name}"
    if isinstance(inner, SVDRegister):
      offset = _resolve_inner_offset(
          inner.address_offset, cluster_proto, f"{where}.{inner.name}")
      yield _build_register(
          inner, peripheral, base_expr, type_qualifier,
          name=_normalize_name(strip_prefix(inner.name)),
          template_params=[cluster_param],
          offset_override=offset)
    elif isinstance(inner, SVDRegisterArray):
      proto = inner.registers[0]
      meta = inner.meta_register
      offset = _resolve_inner_offset(
          proto.address_offset, cluster_proto, f"{where}.{meta.name}")
      array_param = TemplateParam(
          name="ArrayIndex",
          dim=len(inner.registers),
          increment=meta.dim_increment)
      yield _build_register(
          proto, peripheral, base_expr, type_qualifier,
          name=_normalize_name(strip_prefix(_strip_dim_placeholder(meta.name))),
          template_params=[cluster_param, array_param],
          offset_override=offset)
    else:
      raise NotImplementedError(
          f"Peripheral {peripheral.name!r} cluster {cluster_proto.name!r} "
          f"contains an inner element of type {type(inner).__name__} which is "
          f"not yet supported.")


def _build_register(svd, peripheral, base_expr, type_qualifier, *,
                    name, template_params, offset_override=None):
  offset = svd.address_offset if offset_override is None else offset_override
  fields = [_build_field(f) for f in sorted(svd.fields, key=lambda f: f.bit_offset)]
  _validate_register_field_names(fields, name, peripheral.name)
  bits = _register_size(svd)
  slots = _build_slots(fields, bits)
  return Register(
      name=name,
      description=svd.description,
      storage_type=_register_storage_type(bits),
      reset_literal=_register_reset_literal(bits, svd.reset_value or 0),
      access=_access_to_mmio(svd.access),
      addr_expr=_format_addr_expr(peripheral, offset, base_expr, template_params),
      template_params=template_params,
      fields=fields,
      slots=slots)


def _build_field(svd):
  enums = _build_enums(svd)
  return Field(
      name=svd.name,
      description=svd.description,
      offset=svd.bit_offset,
      width=svd.bit_width,
      value_type=_field_value_type(svd, enums),
      access=_access_to_mmio(svd.access),
      modify=_modify_write_to_mmio(
          svd.modified_write_values,
          where=f"{svd.name}"),
      enums=enums)


def _build_enums(svd_field):
  if not (svd_field.is_enumerated_type and svd_field.enumerated_values):
    return []
  out = []
  for enum_set in svd_field.enumerated_values:
    name = enum_set.name or svd_field.name
    values = [
        EnumValue(name=v.name, value=v.value, description=v.description)
        for v in enum_set.enumerated_values]
    out.append(Enum(name=name, values=values))
  return out


_REEXPORT_FALLBACK = "VALUE"


def _validate_register_field_names(fields, register_name, peripheral_name):
  """Ensure cpp_reexport's rename strategy stays unambiguous.

  When a field's name matches the register's name, cpp_reexport re-exports it
  as VALUE to avoid shadowing the injected-class-name. Fail loud if a real
  field already occupies that name (or if the register itself is named VALUE)
  — silent collision would compile but pick the wrong type.
  """
  names = [f.name for f in fields]
  if register_name not in names:
    return
  if register_name == _REEXPORT_FALLBACK:
    raise ValueError(
        f"{peripheral_name}.{register_name}: cannot apply rename fallback — "
        f"the register itself is named {_REEXPORT_FALLBACK!r} so the renamed "
        f"field would still shadow the enclosing struct. Edit the SVD or "
        f"change the rename strategy in Field.cpp_reexport.")
  if _REEXPORT_FALLBACK in names:
    raise ValueError(
        f"{peripheral_name}.{register_name}: rename fallback collision — "
        f"field {register_name!r} shares the register name and would be "
        f"re-exported as {_REEXPORT_FALLBACK!r}, but a field named "
        f"{_REEXPORT_FALLBACK!r} already exists in this register. Edit the "
        f"SVD or change the rename strategy in Field.cpp_reexport.")


def _build_slots(fields, reg_bits):
  """Ordered Field/Reserved list covering every bit of the register, with
  gaps filled by Reserved entries."""
  slots = []
  cursor = 0
  for f in fields:
    if f.offset > cursor:
      slots.append(Reserved(offset=cursor, width=f.offset - cursor))
    slots.append(f)
    cursor = f.offset + f.width
  if cursor < reg_bits:
    slots.append(Reserved(offset=cursor, width=reg_bits - cursor))
  return slots


# =================================================================================================
# Address resolution

def _format_addr_expr(peripheral, offset, base_expr, template_params):
  """Build the address expression embedded into Register<>'s template list.

  Standalone (base_expr is None) -> absolute hex literal.
  Family (base_expr is a str)    -> '{base_expr} + offset' plus optional
                                    template-index strides for arrays/clusters.
  """
  if base_expr is None:
    expr = f"0x{peripheral.base_address + offset:08X}u"
  else:
    expr = f"{base_expr} + 0x{offset:X}u"
  for tp in template_params:
    expr += f" + ({tp.name} * 0x{tp.increment:X}u)"
  return expr


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
      f"<modifiedWriteValues>{name.lower()}</modifiedWriteValues> "
      f"on {where} is not supported by ftl::mmio. "
      f"Supported values: {sorted(_MODIFY_WRITE_TO_MMIO)}.")


def _field_value_type(svd_field, enums):
  if len(enums) == 1:
    return f"e{enums[0].name}"
  w = svd_field.bit_width
  if w == 1:  return "bool"
  if w <= 8:  return "std::uint8_t"
  if w <= 16: return "std::uint16_t"
  return "std::uint32_t"


def _register_size(svd):
  return svd.size if svd.size is not None else 32


def _register_storage_type(bits):
  if bits <= 8:  return "std::uint8_t"
  if bits <= 16: return "std::uint16_t"
  return "std::uint32_t"


def _register_reset_literal(bits, reset):
  digits = {8: 2, 16: 4}.get(bits, 8)
  return f"0x{reset:0{digits}X}u"


def _normalize_name(name):
  """'FOO[3]' → 'FOO_3' for array-element registers."""
  m = re.fullmatch(r"(.+)\[(\d+)\]", name)
  return f"{m.group(1)}_{m.group(2)}" if m else name


def _strip_dim_placeholder(name):
  """Remove the [%s] / %s SVD <dim> placeholder from an array name."""
  return name.replace("[%s]", "").replace("%s", "")
