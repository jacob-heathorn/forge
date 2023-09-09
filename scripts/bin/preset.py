# System pythonmodules
import argparse
import os
import contextlib
import subprocess
import shutil
import signal
from jinja2 import Template
import time
import fnmatch
import re

from helpers import print_green, print_red, Error, pushd

import subprocess

def check_preset(project_root: str, preset: str):
  # Run the command and capture the output
  with pushd(project_root):
    result = subprocess.run(["cmake", "--list-presets"], capture_output=True, text=True, check=True)

  # Regular expression pattern to match the preset names
  pattern = r'\s+"([^"]+)"'
  
  # Find all matches of the pattern in the output string
  matches = re.findall(pattern, result.stdout)

  if preset not in matches:
    Error(f"Invalid preset: {preset}\nAvailable presets: {matches}")

class Preset:
  def __init__(self, name: str, project_root: os.path):
    self.name = name
    self.project_root = project_root
    self.debugger = None # Needs assignment later
    check_preset(project_root, self.name)

  def debug(self, executable: os.path):
    if self.debugger is None:
      Error(f"preset<{self.name}> has not been assigned a debugger")
    self.debugger.debug(executable)

