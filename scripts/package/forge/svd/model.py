"""Model objects wrapping cmsis-svd parser output, walked directly by jinja."""

import re
from dataclasses import dataclass
from typing import Any, Iterator, Optional, Union

from cmsis_svd.model import (
    SVDAccessType,
    SVDRegister,
    SVDRegisterArray,
    SVDRegisterClusterArray,
)

# cmsis-svd inputs are typed as Any: cmsis-svd marks most fields Optional[X]
# per the SVD spec, but we assume well-formed input. Strict typing here would
# add ~30 asserts to satisfy mypy without catching real bugs.


class Peripheral:
  def __init__(self, svd_peripheral: Any, addr_mode: "AddrMode") -> None:
    self._svd = svd_peripheral
    self._addr = addr_mode

  @property
  def name(self) -> str:
    return self._svd.name

  @property
  def description(self) -> Optional[str]:
    return self._svd.description

  @property
  def lower_name(self) -> str:
    return self.name.lower()

  @property
  def type_qualifier(self) -> str:
    return self._addr.type_qualifier

  def registers(self) -> Iterator["Register"]:
    for r in self._svd.registers:
      if isinstance(r, SVDRegister):
        yield Register(r, self._svd, self._addr)
      elif isinstance(r, SVDRegisterArray):
        yield RegisterArray(r, self._svd, self._addr)
      elif isinstance(r, SVDRegisterClusterArray):
        yield from Cluster(r, self._svd, self._addr).expand()
      else:
        raise NotImplementedError(
            f"Peripheral {self._svd.name!r} has a register of type "
            f"{type(r).__name__} which the generator does not yet emit.")


@dataclass(frozen=True)
class FamilyInstance:
  number: int
  name: str
  base_address: int


class PeripheralFamily:
  def __init__(
      self,
      svd_parent: Any,
      family_name: str,
      instances: list[FamilyInstance],
  ) -> None:
    self._parent = Peripheral(svd_parent, AddrMode.symbolic("kBase"))
    self._family_name = family_name
    self._instances = instances

  @property
  def family_name(self) -> str:
    return self._family_name

  @property
  def class_name(self) -> str:
    return self._family_name.capitalize()

  @property
  def lower_name(self) -> str:
    return self._family_name.lower()

  @property
  def instances(self) -> list[FamilyInstance]:
    return self._instances

  @property
  def parent(self) -> Peripheral:
    return self._parent

  def cpp_instance_assert(self) -> str:
    clauses = " || ".join(f"Instance == {i.number}u" for i in self._instances)
    nums = ", ".join(str(i.number) for i in self._instances)
    return (f'static_assert(\n      {clauses},\n'
            f'      "{self.class_name}: Instance must be one of {nums}");')


class Register:
  """A single register declaration. Plain registers have no template params;
  RegisterArray (subclass) and cluster-inner Registers carry an
  Index/ClusterIndex/ArrayIndex."""

  def __init__(
      self,
      svd_register: Any,
      peripheral: Any,
      addr_mode: "AddrMode",
      *,
      name: Optional[str] = None,
      offset: Optional[int] = None,
      template_params: Optional[list["TemplateParam"]] = None,
  ) -> None:
    self._svd = svd_register
    self._peripheral = peripheral
    self._addr_mode = addr_mode
    self._name = name if name is not None else _normalize_name(svd_register.name)
    self._offset = offset if offset is not None else svd_register.address_offset
    self._template_params = template_params if template_params is not None else []
    self._validate_field_names()

  @property
  def name(self) -> str:
    return self._name

  @property
  def description(self) -> Optional[str]:
    return self._svd.description

  @property
  def addr_expr(self) -> str:
    return self._addr_mode.format(self._offset, self._template_params)

  @property
  def storage_type(self) -> str:
    return _storage_type(self._bits)

  @property
  def reset_literal(self) -> str:
    return _reset_literal(self._bits, self._svd.reset_value or 0)

  @property
  def access(self) -> str:
    return _access_to_mmio(self._svd.access)

  @property
  def template_params(self) -> list["TemplateParam"]:
    return self._template_params

  @property
  def is_templated(self) -> bool:
    return bool(self._template_params)

  def cpp_template_decl(self) -> str:
    if not self._template_params:
      return ""
    params = ", ".join(p.cpp_decl() for p in self._template_params)
    return f"template<{params}>"

  def fields(self) -> list["Field"]:
    return [Field(f) for f in sorted(self._svd.fields, key=lambda f: f.bit_offset)]

  def slots(self) -> list[Union["Field", "Reserved"]]:
    """Fields plus Reserved entries filling the bit-gaps. Used for the
    Register<> template arg list, which has to cover every bit."""
    out: list[Union[Field, Reserved]] = []
    cursor = 0
    for f in self.fields():
      if f.offset > cursor:
        out.append(Reserved(cursor, f.offset - cursor))
      out.append(f)
      cursor = f.offset + f.width
    if cursor < self._bits:
      out.append(Reserved(cursor, self._bits - cursor))
    return out

  def enums(self) -> list["Enum"]:
    seen: set[str] = set()
    out: list[Enum] = []
    for f in self.fields():
      for e in f.enums():
        if e.name not in seen:
          seen.add(e.name)
          out.append(e)
    return out

  @property
  def _bits(self) -> int:
    return self._svd.size if self._svd.size is not None else 32

  def _validate_field_names(self) -> None:
    # cpp_reexport renames same-name fields to VALUE; fail loud if that target
    # is taken or if the register itself is named VALUE.
    field_names = [f.name for f in self._svd.fields]
    if self._name not in field_names:
      return
    if self._name == _REEXPORT_FALLBACK:
      raise ValueError(
          f"{self._peripheral.name}.{self._name}: register itself is "
          f"{_REEXPORT_FALLBACK!r}; rename fallback can't help.")
    if _REEXPORT_FALLBACK in field_names:
      raise ValueError(
          f"{self._peripheral.name}.{self._name}: field {self._name!r} would "
          f"be re-exported as {_REEXPORT_FALLBACK!r}, but that name is already "
          f"taken by another field in this register.")


class RegisterArray(Register):
  """A Register array — emitted as a Register templated on Index."""

  def __init__(
      self,
      svd_array: Any,
      peripheral: Any,
      addr_mode: "AddrMode",
  ) -> None:
    proto = svd_array.registers[0]
    meta = svd_array.meta_register
    super().__init__(
        proto, peripheral, addr_mode,
        name=_normalize_name(_strip_dim_placeholder(meta.name)),
        template_params=[TemplateParam(
            "Index", len(svd_array.registers), meta.dim_increment)])


class Cluster:
  """A cluster array — N copies of a group of registers. Not a Register
  itself; expands into one Register per inner entry, each carrying an
  extra ClusterIndex template param."""

  def __init__(
      self,
      svd_cluster_array: Any,
      peripheral: Any,
      addr_mode: "AddrMode",
  ) -> None:
    self._proto = svd_cluster_array.clusters[0]
    self._peripheral = peripheral
    self._addr_mode = addr_mode
    self._cluster_param = TemplateParam(
        "ClusterIndex", len(svd_cluster_array.clusters), self._proto.dim_increment)
    self._prefix = self._proto.name + "_"

  def expand(self) -> Iterator[Register]:
    for inner in self._proto.registers:
      if isinstance(inner, SVDRegister):
        yield self._inner_register(inner)
      elif isinstance(inner, SVDRegisterArray):
        yield self._inner_array(inner)
      else:
        raise NotImplementedError(
            f"Peripheral {self._peripheral.name!r} cluster {self._proto.name!r} "
            f"contains an inner element of type {type(inner).__name__} which is "
            f"not yet supported.")

  def _inner_register(self, inner: Any) -> Register:
    return Register(
        inner, self._peripheral, self._addr_mode,
        name=_normalize_name(self._strip_prefix(inner.name)),
        offset=self._resolve_offset(inner.address_offset, inner.name),
        template_params=[self._cluster_param])

  def _inner_array(self, inner: Any) -> Register:
    proto = inner.registers[0]
    meta = inner.meta_register
    return Register(
        proto, self._peripheral, self._addr_mode,
        name=_normalize_name(self._strip_prefix(_strip_dim_placeholder(meta.name))),
        offset=self._resolve_offset(proto.address_offset, meta.name),
        template_params=[self._cluster_param, TemplateParam(
            "ArrayIndex", len(inner.registers), meta.dim_increment)])

  def _strip_prefix(self, name: str) -> str:
    return name[len(self._prefix):] if name.startswith(self._prefix) else name

  def _resolve_offset(self, raw_offset: int, register_name: str) -> int:
    return _resolve_inner_offset(
        raw_offset, self._proto,
        f"{self._peripheral.name}.{self._proto.name}.{register_name}")


class Field:
  def __init__(self, svd_field: Any) -> None:
    self._svd = svd_field

  @property
  def name(self) -> str:
    return self._svd.name

  @property
  def description(self) -> Optional[str]:
    return self._svd.description

  @property
  def offset(self) -> int:
    return self._svd.bit_offset

  @property
  def width(self) -> int:
    return self._svd.bit_width

  @property
  def value_type(self) -> str:
    enums = self.enums()
    if len(enums) == 1:
      return f"e{enums[0].name}"
    w = self.width
    if w == 1:
      return "bool"
    if w <= 8:
      return "std::uint8_t"
    if w <= 16:
      return "std::uint16_t"
    return "std::uint32_t"

  @property
  def access(self) -> str:
    return _access_to_mmio(self._svd.access)

  @property
  def modify(self) -> str:
    return _modify_write_to_mmio(self._svd.modified_write_values, where=self.name)

  def enums(self) -> list["Enum"]:
    if not (self._svd.is_enumerated_type and self._svd.enumerated_values):
      return []
    return [Enum(es, self.name) for es in self._svd.enumerated_values]

  def cpp_using(self) -> str:
    return (f"using {self.name} = ftl::mmio::Field<"
            f"{self.width}, {self.offset}, {self.value_type}, "
            f"{self.access}, {self.modify}>;")

  def cpp_template_arg(self, register_name: str, type_qualifier: str = "") -> str:
    return f"{type_qualifier}{register_name}_fields_::{self.name}"

  def cpp_reexport(self, register_name: str, type_qualifier: str = "") -> str:
    # Same-name fields (e.g. GPIO::DR) get aliased to VALUE — `using DR = ...`
    # inside `struct DR` would shadow the injected-class-name.
    rhs = f"{type_qualifier}{register_name}_fields_::{self.name}"
    lhs = _REEXPORT_FALLBACK if self.name == register_name else self.name
    return f"using {lhs} = {rhs};"


@dataclass(frozen=True)
class Reserved:
  offset: int
  width: int

  # Unused args mirror Field.cpp_template_arg so jinja calls both uniformly.
  def cpp_template_arg(self, register_name: str = "", type_qualifier: str = "") -> str:
    return f"ftl::mmio::Reserved<{self.width}, {self.offset}>"


class Enum:
  def __init__(self, svd_enum_set: Any, fallback_name: str) -> None:
    self._svd = svd_enum_set
    self._fallback = fallback_name

  @property
  def name(self) -> str:
    return self._svd.name or self._fallback

  def values(self) -> list["EnumValue"]:
    return [EnumValue(v) for v in self._svd.enumerated_values]


class EnumValue:
  def __init__(self, svd_enum_value: Any) -> None:
    self._svd = svd_enum_value

  @property
  def name(self) -> str:
    return self._svd.name

  @property
  def value(self) -> int:
    return self._svd.value

  @property
  def description(self) -> Optional[str]:
    return self._svd.description


@dataclass(frozen=True)
class TemplateParam:
  name: str    # Index | ClusterIndex | ArrayIndex
  dim: int
  increment: int

  def cpp_decl(self) -> str:
    return f"std::uint32_t {self.name}"

  def cpp_static_assert(self, register_name: str) -> str:
    return (f'static_assert({self.name} < {self.dim}u, '
            f'"{register_name}: {self.name} out of range");')


@dataclass(frozen=True)
class AddrMode:
  """Per-peripheral addressing: absolute (standalone) or symbolic kBase (family)."""

  base_address: Optional[int]
  base_symbol: Optional[str]
  type_qualifier: str

  @classmethod
  def absolute(cls, base_address: int) -> "AddrMode":
    return cls(base_address=base_address, base_symbol=None, type_qualifier="")

  @classmethod
  def symbolic(cls, symbol: str) -> "AddrMode":
    return cls(base_address=None, base_symbol=symbol, type_qualifier="typename ")

  def format(self, offset: int, template_params: list[TemplateParam]) -> str:
    if self.base_symbol is None:
      assert self.base_address is not None
      expr = f"0x{self.base_address + offset:08X}u"
    else:
      expr = f"{self.base_symbol} + 0x{offset:X}u"
    for tp in template_params:
      expr += f" + ({tp.name} * 0x{tp.increment:X}u)"
    return expr


def _resolve_inner_offset(inner_offset: int, cluster: Any, where: str) -> int:
  """SVDs disagree on whether a cluster-inner <addressOffset> is cluster- or
  peripheral-relative (NXP MIMXRT DMA TCD style). cmsis-svd always adds the
  cluster offset, double-counting the broken case. We accept whichever
  interpretation uniquely fits the cluster stride; ambiguity or no-fit
  raises."""
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


_REEXPORT_FALLBACK = "VALUE"


_ACCESS_TO_MMIO = {
    SVDAccessType.READ_ONLY: "ftl::mmio::RO",
    SVDAccessType.WRITE_ONLY: "ftl::mmio::WO",
    SVDAccessType.WRITE_ONCE: "ftl::mmio::WO",
    SVDAccessType.READ_WRITE: "ftl::mmio::RW",
    SVDAccessType.READ_WRITE_ONCE: "ftl::mmio::RW",
}


def _access_to_mmio(access: Any) -> str:
  return _ACCESS_TO_MMIO.get(access, "ftl::mmio::RW")


_MODIFY_WRITE_TO_MMIO = {
    "ONE_TO_CLEAR": "ftl::mmio::OneToClear",
    "ONE_TO_SET": "ftl::mmio::OneToSet",
    "ONE_TO_TOGGLE": "ftl::mmio::OneToToggle",
    "MODIFY": "ftl::mmio::Normal",
}


def _modify_write_to_mmio(mwv: Any, *, where: str) -> str:
  if mwv is None:
    return "ftl::mmio::Normal"
  name = getattr(mwv, "name", str(mwv))
  if name in _MODIFY_WRITE_TO_MMIO:
    return _MODIFY_WRITE_TO_MMIO[name]
  raise NotImplementedError(
      f"<modifiedWriteValues>{name.lower()}</modifiedWriteValues> on {where} "
      f"is not supported by ftl::mmio. "
      f"Supported values: {sorted(_MODIFY_WRITE_TO_MMIO)}.")


def _storage_type(bits: int) -> str:
  if bits <= 8:
    return "std::uint8_t"
  if bits <= 16:
    return "std::uint16_t"
  return "std::uint32_t"


def _reset_literal(bits: int, reset: int) -> str:
  digits = {8: 2, 16: 4}.get(bits, 8)
  return f"0x{reset:0{digits}X}u"


def _normalize_name(name: str) -> str:
  # FOO[3] → FOO_3
  m = re.fullmatch(r"(.+)\[(\d+)\]", name)
  return f"{m.group(1)}_{m.group(2)}" if m else name


def _strip_dim_placeholder(name: str) -> str:
  # FOO[%s] / FOO%s → FOO
  return name.replace("[%s]", "").replace("%s", "")
