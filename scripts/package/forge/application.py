import os
import json
import fnmatch
import forge
import subprocess
from abc import ABC, abstractmethod

PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")
CMAKE_PRESETS_JSON = os.path.join(PROJECT_ROOT, "CMakePresets.json")


def split_preset_application(preset_application: str):
  # Split the string into two parts at the first colon
  parts = preset_application.split(':', 1)  # '1' is the maxsplit argument

  # Assign the parts to respective variables
  preset = parts[0]  # The part before the colon
  # The part after the colon, or None if no colon
  application = parts[1] if len(parts) > 1 else None

  return preset, application

def resolve_bin_dir(preset_name: str) -> str:

  with open(CMAKE_PRESETS_JSON, "r") as f:
    data = json.load(f)

  binary_dir = next(
      (preset.get("binaryDir") for preset in data["configurePresets"] if preset["name"] == preset_name),
      None,
  )

  # Expand cmake environment variables.
  binary_dir = binary_dir.replace("${sourceDir}", os.path.dirname(CMAKE_PRESETS_JSON))
  binary_dir = binary_dir.replace("${presetName}", preset_name)

  return binary_dir

def find_application(application_name: str, dir: os.path) -> str:
  matched_files = []

  # Create patterns to match both with and without .elf extension
  patterns = [application_name, f"{application_name}.elf"]

  for root, _, files in os.walk(dir):
    for file in files:
      for pattern in patterns:
        if fnmatch.fnmatch(file, pattern):
          matched_files.append(os.path.join(root, file))

  if len(matched_files) < 1:
    forge.error(f"Runnable '{application_name}' DNE")

  if len(matched_files) > 1:
    forge.error(f"More than one runnable found with the name '{application_name}'")

  application_fullfile = matched_files[0]
  return application_fullfile

def print_size(fullfile):
  subprocess.check_call(['size', fullfile])

class Application(ABC):
  def __init__(self, preset_application: str):
    self.preset_name, self.application_name = split_preset_application(preset_application)
    self.bin_dir = resolve_bin_dir(self.preset_name)
    self.application_fullfile = find_application(self.application_name, self.bin_dir)

  @abstractmethod
  def run(self):
    pass

  @abstractmethod
  def debug(self):
    pass

class NativeApplication(Application):
  def __init__(self, preset_application: str):
    super().__init__(preset_application)

  def run(self):
    forge.print_green(f"Runnig application {self.application_fullfile}")
    print_size(self.application_fullfile)
    args = [self.application_fullfile]
    subprocess.check_call(args)
    forge.print_green("Success!")

  def debug(self):
    debugger = forge.NativeDebugger(name=f"{self.preset_name}:{self.application_name}",
                                    project_root=PROJECT_ROOT, gdb=NATIVE_GDB_PATH)
    debugger.debug(self.application_fullfile)
