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


def find_application(application_name: str, dir: os.path):
  matched_files = []

  # Create patterns to match both with and without .elf extension
  patterns = [application_name, f"{application_name}.elf"]

  for root, _, files in os.walk(dir):
      for file in files:
          for pattern in patterns:
              if fnmatch.fnmatch(file, pattern):
                  matched_files.append(os.path.join(root, file))

  if len(matched_files) < 1:
    error(f"Runnable '{application_name}' DNE")

  if len(matched_files) > 1:
    error(f"More than one runnable found with the name '{application_name}'")
  
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

    # CMake generator (-G)
    self.generator = "Ninja"
    
    # Cache variables (-D)
    self.cmake_toolchain_file = None
    self.cmake_export_compile_commands = "YES"

  def clean(self):
    # Remove the release build directory if it exists
    if os.path.exists(self.bin_dir(release=True)):
      shutil.rmtree(self.bin_dir(release=True))

    # Remove the debug build directory if it exists
    if os.path.exists(self.bin_dir(release=False)):
      shutil.rmtree(self.bin_dir(release=False))

  def build(self, release: bool, verbose: bool):
    # Create a new directory with this preset name wherver we are right now.
    with pushd(self.name):
      # Configure
      args = ['cmake', '-G', self.generator, self.project_root
              , f'-DCMAKE_TOOLCHAIN_FILE={self.cmake_toolchain_file}'
              , f'-DCMAKE_BUILD_TYPE={cmake_build_type(release)}'
              , f'-DCMAKE_EXPORT_COMPILE_COMMANDS={self.cmake_export_compile_commands}'
              ]
      
      if verbose:
        args.append('-DCMAKE_VERBOSE_MAKEFILE=ON')
      
      subprocess.check_call(args)

      # Build
      args = ['cmake', '--build', '.']
      subprocess.check_call(args)

  # def bin_dir(self, release: bool):
  #   if release:
  #     return os.path.join(self.top_build_root, f"{self.name}-release")
  #   else:
  #     return os.path.join(self.top_build_root, f"{self.name}-debug")


  # def run(self, executable: str, release: bool):
  #   fullfile = find_runnable(executable, self._preset_build_root(release))
  #   self.debugger.run(fullfile)
  #   print_size(fullfile)
  #   print

  # def debug(self, executable: str):
  #   self._check_debugger()
  #   fullfile = find_runnable(executable, self._preset_build_root(debug=True))
  #   print_size(fullfile)
  #   self.debugger.debug(fullfile)
  
  # def flash(self, binary: str, debug: bool):
  #   if self.flasher is None:
  #     error(f"dude, preset<{self.name}> has not been assigned a flasher.")
  #   fullfile = find_runnable(binary, self._preset_build_root(debug))
  #   self.flasher.flash(fullfile)

