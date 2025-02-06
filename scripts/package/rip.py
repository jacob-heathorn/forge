#!/usr/bin/env python3
#
# Top-level repository build tool.

# System pythonmodules
import argparse
import os
import shutil
import forge

# Pull in environment variables
FORGE_ROOT = os.environ.get("FORGE_ROOT")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")


def main():
  parser = argparse.ArgumentParser(description='Repository build driver')
  parser.add_argument(
      '-c',
      '--clean',
      action='store_true',
      default=False,
      help='Delete the build folder')
  parser.add_argument('-r', '--run', type=str, help='Run <preset:application> natively')
  parser.add_argument('-d', '--debug', type=str, help='Debug <preset:application> natively')
  args = parser.parse_args()

  # Do clean
  if args.clean:
    shutil.rmtree(os.path.join(PROJECT_ROOT, '.bin'), ignore_errors=True)
    forge.remove_file(os.path.join(PROJECT_ROOT, '.vscode', 'launch.json'))

  # Do run
  if args.run:
    forge.NativeApplication(args.run).run()

  # Do debug
  if args.debug:
    forge.NativeApplication(args.debug).debug()


if __name__ == '__main__':
  main()
