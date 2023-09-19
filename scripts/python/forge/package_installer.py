# System pythonmodules
import subprocess
import semver
import re

# Custom imports
from forge.helpers import error, pushd, GREEN_CHECK

GREEN_CHECK = "\033[32m\u2713\033[0m"

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
# Package Installer

class PackageInstaller:
  def __init__(self, update_args):
    self.update_args = update_args
    self.updated = False

  def update_only_once(self):
    if not self.updated:
      if self.update_args:
        subprocess.check_call(self.update_args)

  def install(self, name: str, version_args: [], install_args: [], range: str):
    print(f"\nChecking {name} install...")

    installed_ver = find_semver(version_args)
    if installed_ver:
      # Check if a version is compatible with a range
      if semver.match(installed_ver, range):
        print(f" - Installed version {installed_ver} is compatible with range {range} {GREEN_CHECK}")
      else:
        error(f" - Installed version {installed_ver} is incompatible with range {range}")
    
    else:
      print(f"Installing {name}...")
      self.update_only_once()
      subprocess.check_call(install_args)
      installed_ver = find_semver(version_args)
      if installed_ver:
        print(f" - Successfully installed version {installed_ver} {GREEN_CHECK}")
      else:
        error(f"Failed to install {name}")


class AptInstaller(PackageInstaller):
  def __init__(self):
    super().__init__(update_args = ['sudo', 'apt', 'update'])

  def install(self, name: str, version_args: [], install_args: [], range: str):
    install_args = ['sudo', 'apt', 'install'] + install_args
    super().install(name, version_args, install_args, range)

class SnapInstaller(PackageInstaller):
  def __init__(self):
    super().__init__(update_args = [])

  def install(self, name: str, version_args: [], install_args: [], range: str):
    install_args = ['sudo', 'snap', 'install'] + install_args
    super().install(name, version_args, install_args, range)
