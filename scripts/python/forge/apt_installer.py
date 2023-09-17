# System pythonmodules
import subprocess
import os
import shutil
import semver
import re

# Custom imports
from forge.helpers import error, pushd

def command_get_output(args):
  try:
    output = subprocess.check_output(args, text=True)
    output = output.strip()
    return output
  except subprocess.CalledProcessError as e:
    error(f"Command failed with error {e.returncode}")
  except FileNotFoundError:
    error("Command not found")

def check_installed(args):
  try:
    output = subprocess.check_output(args, text=True)
    output = output.strip()
    return output
  except subprocess.CalledProcessError as e:
    return False
  except FileNotFoundError:
    return False

def find_semver(args):
  # The regex pattern for semantic versioning
  pattern = r'\b\d+\.\d+\.\d+(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?(\+[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?\b'

  if not check_installed(args):
    return None
  
  # Search for the pattern
  match = re.search(pattern, command_get_output(args))
  
  if match:
      # Return the first found semantic version
      return match.group(0)
  else:
      return None

# =================================================================================================
# Apt Installer

class AptInstaller:
  def __init__(self):
    self.updated = False
    self.prepend_args = ['sudo', 'apt']

  def update_only_once(self):
    if not self.updated:
      args = self.prepend_args + ['update']
      subprocess.check_call(args)
      self.updated = True



  def install(self, name: str, command: str, range: str):
    print(f"Checking {name} install...")

    version_args = self.prepend_args + [command, '--version']
    installed_ver = find_semver(version_args)
    if installed_ver:
      # Check if a version is compatible with a range
      if semver.match(installed_ver, range):
        print(f" - Installed version {installed_ver} is compatible with range {range}")
      else:
        error(f" - Installed version {installed_ver} is incompatible with range {range}")
    
    else:
      print(f"Installing {name}...")
      self.update_only_once()
      install_args = self.prepend_args + ['install', name]
      subprocess.check_call(install_args)
      installed_ver = find_semver(version_args)
      if installed_ver:
        print(f" - Installed version {installed_ver}")
      else:
        error("Failed to install ninja")
