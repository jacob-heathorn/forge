# System pythonmodules
import os
import subprocess
import shutil
from jinja2 import Template
import fnmatch
import re

from helpers import print_green, print_red, Error, pushd

import subprocess

def list_presets(project_root: str):
  # Run the command and capture the output
  with pushd(project_root):
    result = subprocess.run(["cmake", "--list-presets"], capture_output=True, text=True, check=True)

  # Regular expression pattern to match the preset names
  pattern = r'\s+"([^"]+)"'
  
  # Find all matches of the pattern in the output string
  return re.findall(pattern, result.stdout)

def check_preset(project_root: str, preset: str):
  presets = list_presets(project_root)

  if preset not in presets:
    Error(f"Invalid preset: {preset}\nAvailable presets: {presets}")

class Preset:
  def __init__(self, name: str, project_root: os.path):
    self.name = name
    self.project_root = project_root
    self.top_build_root = os.path.join(project_root, 'bin')
    self.preset_build_root = os.path.join(self.top_build_root, name)
    self.debugger = None # Needs assignment later
    check_preset(project_root, self.name)

  def clean(self):
    # Remove the preset's build dir
    if os.path.exists(self.preset_build_root):
      shutil.rmtree(self.preset_build_root)
    
    # If the build dir is empty, delete it
    if not bool(os.listdir(self.top_build_root)):
      shutil.rmtree(self.top_build_root)

  def build(self, verbose: bool):
    with pushd(self.project_root):
      # Configure
      args = ['cmake', f'--preset={self.name}']
      
      if verbose:
        args.append('-DCMAKE_VERBOSE_MAKEFILE=ON')
      
      subprocess.check_call(args)

      # Build
      args = ['cmake', '--build', f'--preset={self.name}']
      subprocess.check_call(args)

  def run(self, executable: str):
    fullfile = self._find_executable(executable)
    self.debugger.run(fullfile)

  def debug(self, executable: str):
    self._check_debugger()
    fullfile = self._find_executable(executable)
    self.debugger.debug(fullfile)

  # Private methods
  #
  def _find_executable(self, executable_name: str):
    matched_files = []

    for root, _, files in os.walk(self.preset_build_root):
        for file in files:
            if fnmatch.fnmatch(file, executable_name):
                matched_files.append(os.path.join(root, file))

    if len(matched_files) < 1:
      Error(f"Runnable '{executable_name}' DNE")

    if len(matched_files) > 1:
      Error(f"More than one executable found with the name '{executable_name}'")
    
    return matched_files[0]
  
  def _check_debugger(self):
    if self.debugger is None:
      Error(f"preset<{self.name}> has not been assigned a debugger")
