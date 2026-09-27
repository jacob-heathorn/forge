"""Writes a binary file as a C++ source defining `unsigned char <symbol>[]` and `<symbol>_len`."""

import sys
from typing import Final

_BYTES_PER_LINE: Final = 12


def main() -> None:
  symbol, source, destination = sys.argv[1:]
  with open(source, "rb") as file:
    data = file.read()
  rows = [data[i:i + _BYTES_PER_LINE] for i in range(0, len(data), _BYTES_PER_LINE)]
  lines = [f"unsigned char {symbol}[] = {{"]
  lines += ["  " + ", ".join(f"0x{byte:02x}" for byte in row) + "," for row in rows]
  lines += ["};", f"unsigned int {symbol}_len = {len(data)};", ""]
  with open(destination, "w") as file:
    file.write("\n".join(lines))


if __name__ == "__main__":
  main()
