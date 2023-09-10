# System pythonmodules
import os
import subprocess
import shutil
import fnmatch

from helpers import Error, pushd

def cmake_build_type(debug: bool):
  if debug:
    return "Debug"
  else:
    return "Release"


def find_executable(executable_name: str, dir: os.path):
  matched_files = []

  for root, _, files in os.walk(dir):
      for file in files:
          if fnmatch.fnmatch(file, executable_name):
              matched_files.append(os.path.join(root, file))

  if len(matched_files) < 1:
    Error(f"Runnable '{executable_name}' DNE")

  if len(matched_files) > 1:
    Error(f"More than one executable found with the name '{executable_name}'")
  
  return matched_files[0]

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
    
    # If the root build directory is empty, remove it
    if os.path.exists(self.top_build_root):
      if not bool(os.listdir(self.top_build_root)):
        shutil.rmtree(self.top_build_root)

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
    fullfile = find_executable(executable, self._preset_build_root(debug=False))
    self.debugger.run(fullfile)

  def debug(self, executable: str):
    self._check_debugger()
    fullfile = find_executable(executable, self._preset_build_root(debug=True))
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
      Error(f"preset<{self.name}> has not been assigned a debugger")
