# System pythonmodules
import os
import subprocess
import shutil
import fnmatch

from forge.helpers import error, pushd

# =================================================================================================
# Preset helpers

def cmake_build_type(debug: bool):
  if debug:
    return "Debug"
  else:
    return "Release"


def find_runnable(runnable_name: str, dir: os.path):
  matched_files = []

  # Create patterns to match both with and without .elf extension
  patterns = [runnable_name, f"{runnable_name}.elf"]

  for root, _, files in os.walk(dir):
      for file in files:
          for pattern in patterns:
              if fnmatch.fnmatch(file, pattern):
                  matched_files.append(os.path.join(root, file))

  if len(matched_files) < 1:
    error(f"Runnable '{runnable_name}' DNE")

  if len(matched_files) > 1:
    error(f"More than one runnable found with the name '{runnable_name}'")
  
  return matched_files[0]

def subset_presets(subset_names, superset_presets):

  subset = []
  for preset in superset_presets:
    if preset.name in subset_names:
      subset.append(preset)
  
  return subset

def print_size(fullfile):
  print(f'\nSize information:')
  subprocess.check_call(['size', fullfile])
  print()

# =================================================================================================
# Preset - Encapsulates a cmake preset

class Preset:
  def __init__(self, name: str, project_root: os.path):
    self.name = name
    self.project_root = project_root
    self.top_build_root = os.path.join(project_root, 'bin')
    self.debugger = None # Needs assignment later

    # CMake generator (-G)
    self.generator = "Ninja"
    
    # Cache variables (-D)
    self.cmake_toolchain_file = None
    self.cmake_export_compile_commands = "YES"

  def clean(self):
    # Remove the debug build directory if it exists
    if os.path.exists(self._preset_build_root(debug=True)):
      shutil.rmtree(self._preset_build_root(debug=True))

    # Remove the release build directory if it exists
    if os.path.exists(self._preset_build_root(debug=False)):
      shutil.rmtree(self._preset_build_root(debug=False))

  def build(self, debug: bool, verbose: bool):
    with pushd(self._preset_build_root(debug)):
      # Configure
      args = ['cmake', '-G', self.generator, self.project_root
              , f'-DCMAKE_TOOLCHAIN_FILE={self.cmake_toolchain_file}'
              , f'-DCMAKE_BUILD_TYPE={cmake_build_type(debug)}'
              , f'-DCMAKE_EXPORT_COMPILE_COMMANDS={self.cmake_export_compile_commands}'
             ]
      
      if verbose:
        args.append('-DCMAKE_VERBOSE_MAKEFILE=ON')
      
      subprocess.check_call(args)

      # Build
      args = ['cmake', '--build', self._preset_build_root(debug)]
      subprocess.check_call(args)

  def run(self, executable: str):
    fullfile = find_runnable(executable, self._preset_build_root(debug=False))
    self.debugger.run(fullfile)
    print_size(fullfile)

  def debug(self, executable: str):
    self._check_debugger()
    fullfile = find_runnable(executable, self._preset_build_root(debug=True))
    print_size(fullfile)
    self.debugger.debug(fullfile)

  # Private methods
  # 
  def _preset_build_root(self, debug: bool):
    if debug:
      return os.path.join(self.top_build_root, f"{self.name}-debug")
    else:
      return os.path.join(self.top_build_root, f"{self.name}-release")
  
  def _check_debugger(self):
    if self.debugger is None:
      error(f"preset<{self.name}> has not been assigned a debugger")
