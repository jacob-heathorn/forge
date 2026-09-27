"""Forge tooling. Submodules load on first use so tools import only what they need."""

import importlib
from types import ModuleType
from typing import Any

_EXPORTS = {
    "Application": "application",
    "NativeApplication": "application",
    "NativeDebugger": "debugger",
    "RegisterGenerator": "svd",
    "SerialTerminal": "serial_terminal",
    "error": "helpers",
    "print_green": "helpers",
    "print_red": "helpers",
    "pushd": "helpers",
    "remove_file": "helpers",
}


def __getattr__(name: str) -> Any:
  if name in _EXPORTS:
    return getattr(_submodule(_EXPORTS[name]), name)
  return _submodule(name)


def _submodule(name: str) -> ModuleType:
  try:
    return importlib.import_module(f".{name}", __name__)
  except ModuleNotFoundError as e:
    if e.name != f"{__name__}.{name}":
      raise
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}") from None
