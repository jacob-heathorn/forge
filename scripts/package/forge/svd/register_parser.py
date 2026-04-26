"""Parse a CMSIS-SVD file into our Peripheral / PeripheralFamily model."""

import os
import pickle
import re
from pathlib import Path
from typing import Any, Optional

from cmsis_svd.parser import SVDParser

from .model import AddrMode, FamilyInstance, Peripheral, PeripheralFamily


PROJECT_ROOT = Path(os.environ.get("PROJECT_ROOT", ""))
BIN_DIR = PROJECT_ROOT / ".bin"


def parse(svd_file: str) -> tuple[list[Peripheral], list[PeripheralFamily]]:
  """Parse svd_file (with on-disk cache) and return (standalone, families)."""
  device = _load_device(svd_file)
  return _group_peripherals(device)


def _load_device(svd_file: str) -> Any:
  # Cache invalidation by mtime: edits to test fixtures or vendor SVDs would
  # otherwise silently reuse a stale parse.
  cache_path = BIN_DIR / f"{Path(svd_file).stem}.pkl"
  if cache_path.exists() and cache_path.stat().st_mtime >= os.path.getmtime(svd_file):
    print(f"Loading cached device from {cache_path}...")
    with cache_path.open("rb") as f:
      return pickle.load(f)
  print(f"Parsing {svd_file}...")
  device = SVDParser.for_xml_file(svd_file).get_device()
  BIN_DIR.mkdir(parents=True, exist_ok=True)
  with cache_path.open("wb") as f:
    pickle.dump(device, f)
  return device


def _group_peripherals(
    device: Any,
) -> tuple[list[Peripheral], list[PeripheralFamily]]:
  by_canonical: dict[str, list[Any]] = {}
  for p in device.peripherals:
    by_canonical.setdefault(p.derived_from or p.name, []).append(p)

  standalone: list[Peripheral] = []
  families: list[PeripheralFamily] = []
  for canonical_name, members in by_canonical.items():
    if len(members) < 2:
      standalone.append(_standalone(members[0]))
      continue
    fam, orphans = _family(canonical_name, members)
    if fam is None:
      standalone.extend(_standalone(m) for m in members)
      continue
    families.append(fam)
    standalone.extend(_standalone(m) for m in orphans)
  return standalone, families


def _standalone(svd_peripheral: Any) -> Peripheral:
  return Peripheral(svd_peripheral, AddrMode.absolute(svd_peripheral.base_address))


def _family(
    canonical_name: str,
    members: list[Any],
) -> tuple[Optional[PeripheralFamily], list[Any]]:
  """family is None unless at least 2 members have a digit suffix; orphans
  are members without one."""
  family_name = re.sub(r"\d+$", "", canonical_name) or canonical_name
  instances: list[FamilyInstance] = []
  orphans: list[Any] = []
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
