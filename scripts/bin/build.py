#!/usr/bin/env python3
#
# Top-level repository build tool.

# System pythonmodules
import argparse
import os
import subprocess
import shutil
import signal
from jinja2 import Template
import time
import fnmatch

from forge import Preset, NativeDebugger
from helpers import print_green, print_red, Error, pushd

# # Custom python modules
# from OpenOcdServer import OpenOcdServer

FORGE_ROOT = os.environ.get("FORGE_ROOT")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
BUILD_DIR_ROOT = os.path.join(PROJECT_ROOT, 'bin')

# PRESETS_ROOT = os.path.join(FORGE_ROOT, 'presets')
# PRESETS = os.listdir(PRESETS_ROOT)

# TOOLCHAIN_FILE = os.path.join(PROJECT_ROOT, 'firmware', 'stm32h743zi2', 'platform', 'cmake', 'toolchain.cmake')

# OPENOCD_DIR = os.environ.get("OPENOCD_DIR")
# OPEN_OCD_LOGFILE = os.path.join(PROJECT_ROOT,'tools', 'logs', 'openocd_output.log')
# OPENOCD_CONFIG = os.path.join(PROJECT_ROOT, 'tools', 'openocd_server.cfg')

LAUNCH_JSON_JINJA_TEMPLATE = os.path.join(PROJECT_ROOT, 'scripts', 'launch.json.jinja2')
ARM_GDB_PATH = os.environ.get("ARM_GDB_PATH")
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")
VSCODE_FOLDER = os.path.join(PROJECT_ROOT, '.vscode')
GENERATED_LAUNCH_JSON = os.path.join(VSCODE_FOLDER, 'launch.json')

# Returns the Release or Debug build dir.
def BUILD_DIR(preset: str):
  build_dir = os.path.join(BUILD_DIR_ROOT, preset)
  return build_dir

# # Returns the full gdb path, which depends on the preset
# def GDB_PATH(preset: str):
  

# Generates the launch.json file for debugging
def generate_vscode_launch_json(executable_fullfile, gdb_path):
  # Read the template content
    with open(LAUNCH_JSON_JINJA_TEMPLATE, 'r') as file:
        template_content = file.read()

    # Create a Jinja Template instance with the content
    template = Template(template_content)

    # Render the template
    rendered_template = template.render(executable_fullfile=executable_fullfile, gdb_path=gdb_path)

    # Create the .vscode directory if it doesn't exist
    if not os.path.exists(VSCODE_FOLDER):
      os.makedirs(VSCODE_FOLDER)

    # Write the rendered template to a file
    with open(GENERATED_LAUNCH_JSON, 'w') as f:
      f.write(rendered_template)

# Finds the executable in the build directory with the given name.
def find_executable(preset: str, executable_name: str, extension: str):
  if not executable_name.endswith(extension):
    executable_name += extension
  
  matched_files = []

  for root, _, files in os.walk(BUILD_DIR(preset)):
      for file in files:
          if fnmatch.fnmatch(file, executable_name):
              matched_files.append(os.path.join(root, file))

  if len(matched_files) < 1:
    Error(f"Runnable '{executable_name}' DNE")

  if len(matched_files) > 1:
    Error(f"More than one executable found with the name '{executable_name}'")
  
  return matched_files[0]


#==================================================================================================
# User-facing build tool functions
#==================================================================================================

# Cleans the build directory.
def clean():
  if os.path.exists(BUILD_DIR_ROOT):
    shutil.rmtree(BUILD_DIR_ROOT)


# Builds all targets.
def build(preset: str, verbose=False):
  
  with pushd(PROJECT_ROOT):
    # Configure
    args = ['cmake', f'--preset={preset}']
    
    if verbose:
      args.append('-DCMAKE_VERBOSE_MAKEFILE=ON')
    
    subprocess.check_call(args)

    # Build
    args = ['cmake', '--build', f'--preset={preset}']
    subprocess.check_call(args)


# TODO need to delegate to platform class for running/debugging
def run(executable_fullfile):
  print_green(f"Runnig executable {executable_fullfile}")
  args = [executable_fullfile]
  subprocess.check_call(args)
  # with OpenOcdServer(OPENOCD_DIR, OPENOCD_CONFIG, OPEN_OCD_LOGFILE) as server:
  #   server.run(executable_fullfile)
  # time.sleep(.2) # Allows console output to finish
  print_green("Success!")


# Flashes the executable and puts it in a halted state, then waits. Once we
# connect with gdb, execution is automatically resumed.
def debug(executable_fullfile):
  print_green(f"Debugging executable {executable_fullfile}")
  generate_vscode_launch_json(executable_fullfile, NATIVE_GDB_PATH)

  # with OpenOcdServer(OPENOCD_DIR, OPENOCD_CONFIG, OPEN_OCD_LOGFILE) as server:
  #   server.debug(executable_fullfile)
  #   print_green(f"Generating {GENERATED_LAUNCH_JSON}")
  #   generate_vscode_launch_json(executable_fullfile)
  #   time.sleep(.2) # Allows console output to finish
  #   print_green("Ready, connect with gdb to localhost:3333")
  #   # Sleep to allow a user to debug. They'll control+c to get out.
  #   time.sleep(60*60*2)


def main():
  parser = argparse.ArgumentParser(description='Repository build driver')
  parser.add_argument('-p', '--preset', dest="preset", required=False, help='CMake build preset')
  parser.add_argument('-c', '--clean', action='store_true', default=False, help='Delete the build folder')
  parser.add_argument('-b', '--build', action='store_true', default=False, help='Compile')
  parser.add_argument('-r', '--run', dest="runnable", required=False, help='Run the executable with the given name')
  parser.add_argument('-d', '--debug', action='store_true', default=False, help='Debug (attach) with gdb')
  parser.add_argument('-v', '--verbose', action='store_true', default=False, help='Build verbose')
  args = parser.parse_args()

  preset = Preset(args.preset, PROJECT_ROOT)
  if preset.name == "native-debug" or preset.name == "native-release":
    preset.set_debugger(NativeDebugger(preset, NATIVE_GDB_PATH))

  # If no other actions are passed, default to --build.
  if not any([args.clean, args.build, args.runnable]):
    args.build = True

  # Clean first.
  if args.clean:
    clean()

  # Build
  if args.build:
    build(preset=args.preset, verbose=args.verbose)

  # Run.
  if args.runnable:
    runnable_fullfile = find_executable(preset=args.preset, executable_name=args.runnable, extension="")
    
    if args.debug:
      preset.debug(runnable_fullfile)
    else:
      run(runnable_fullfile)


if __name__ == '__main__':
  main()
