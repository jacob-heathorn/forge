#!/usr/bin/env python3
#
# Top-level repository build tool.

# System pythonmodules
import argparse
import os

from preset import Preset
from debugger import NativeDebugger
from helpers import Error

FORGE_ROOT = os.environ.get("FORGE_ROOT")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")


def define_presets():
  presets = []
  # Native
  native = Preset("native", PROJECT_ROOT)
  native.cmake_toolchain_file = os.path.join(FORGE_ROOT, 'platforms','native','toolchain.cmake')
  native.debugger = NativeDebugger(native.name, native.project_root, NATIVE_GDB_PATH)
  presets.append(native)
  return presets

ALL_PRESETS = define_presets()

def subset_presets(subset_names):

  subset = []
  for preset in ALL_PRESETS:
    if preset.name in subset_names:
      subset.append(preset)
  
  return subset

def main():
  parser = argparse.ArgumentParser(description='Repository build driver')
  parser.add_argument('-p', '--presets', dest="presets", required=False, nargs='+', help='CMake build preset(s)')
  parser.add_argument('-c', '--clean', action='store_true', default=False, help='Delete the build folder')
  parser.add_argument('-b', '--build', action='store_true', default=False, help='Compile')
  parser.add_argument('-r', '--run', dest="runnable", required=False, help='Run the executable with the given name')
  parser.add_argument('-d', '--debug', action='store_true', default=False, help='Debug the executable specified by -r')
  parser.add_argument('-v', '--verbose', action='store_true', default=False, help='Build verbose')
  args = parser.parse_args()

  # Don't require a preset, default to all presets if one is not specified
  if args.presets is None:
    presets = ALL_PRESETS
  else:
    presets = subset_presets(args.presets)

  # If no other actions are passed, default to --build.
  if not any([args.clean, args.build, args.runnable]):
    args.build = True

  # Do clean
  if args.clean:
    for preset in presets:
      preset.clean()

  # Do build
  if args.build:
    for preset in presets:
      preset.build(args.debug, args.verbose)

  # Do run/debug
  if args.runnable:
    if len(presets) != 1:
      Error(f"Easy now, run one preset at a time!")

    for preset in presets:
      if args.debug:
        preset.debug(args.runnable)
      else:
        preset.run(args.runnable)

if __name__ == '__main__':
  main()
