#!/usr/bin/env python3
#
# Top-level repository build tool.

# System pythonmodules
import argparse
import os
import shutil

# Custom imports
from forge.preset import Preset, subset_presets
from forge.debugger import NativeDebugger
from forge.helpers import error

# Pull in environment variables
FORGE_ROOT = os.environ.get("FORGE_ROOT")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")

# Define presets
native = Preset("native", PROJECT_ROOT)
native.cmake_toolchain_file = os.path.join(FORGE_ROOT, 'cmake', 'native', 'toolchain.cmake')
native.debugger = NativeDebugger(native.name, native.project_root, NATIVE_GDB_PATH)
ALL_PRESETS = [native]


def main():
  parser = argparse.ArgumentParser(description='Repository build driver')
  parser.add_argument(
      '-p',
      '--presets',
      dest="presets",
      required=False,
      nargs='+',
      help='CMake build preset(s)')
  parser.add_argument(
      '-c',
      '--clean',
      action='store_true',
      default=False,
      help='Delete the build folder')
  parser.add_argument('-b', '--build', action='store_true', default=False, help='Compile')
  parser.add_argument('-r', '--run', dest="runnable", required=False,
                      help='Run the executable with the given name')
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='Debug the executable specified by -r')
  parser.add_argument('-v', '--verbose', action='store_true', default=False, help='Build verbose')
  args = parser.parse_args()

  # Don't require a preset, default to all presets if one is not specified
  if args.presets is None:
    presets = ALL_PRESETS
  else:
    presets = subset_presets(args.presets, ALL_PRESETS)

  # If no other actions are passed, default to --build.
  if not any([args.clean, args.build, args.runnable]):
    args.build = True

  # Do clean
  if args.clean:
    if presets == ALL_PRESETS:
      if os.path.exists(os.path.join(PROJECT_ROOT, 'bin')):
        shutil.rmtree(os.path.join(PROJECT_ROOT, 'bin'))

    else:
      for preset in presets:
        preset.clean()

  # Do build
  if args.build:
    for preset in presets:
      preset.build(args.debug, args.verbose)

  # Do run/debug
  if args.runnable:
    if len(presets) != 1:
      error("Easy now, run one preset at a time!")

    for preset in presets:
      if args.debug:
        preset.debug(args.runnable)
      else:
        preset.run(args.runnable)


if __name__ == '__main__':
  main()
