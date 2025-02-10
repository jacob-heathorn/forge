import os
import json
import fnmatch
import forge
import subprocess
from abc import ABC, abstractmethod
from pathlib import Path
import re

PROJECT_ROOT = os.environ.get("PROJECT_ROOT", "")
CMAKE_PRESETS_JSON = os.path.join(PROJECT_ROOT, "CMakePresets.json")


def check_preset(preset_name):
  with forge.pushd(PROJECT_ROOT):
    try:
      # Run the 'cmake --list-presets' command
      result = subprocess.run(
          ["cmake", "--list-presets"],
          text=True,  # Ensures output is returned as a string
          capture_output=True,  # Captures stdout and stderr
          check=True  # Raises CalledProcessError if the command fails
      )
      # Check if the preset_name is in the output
      if preset_name in result.stdout:
        return True
      else:
        forge.error(f"Preset '{preset_name}' does not exist.\n{result.stdout}")

    except subprocess.CalledProcessError as e:
      forge.error(f"Error running cmake: {e.stderr}")
  return False


def split_preset_application(preset_application: str):
  # Split the string into two parts at the first colon
  parts = preset_application.split(':', 1)  # '1' is the maxsplit argument

  if len(parts) != 2:
    forge.error(f"Incorrect formatting <{preset_application}>, expected <preset:application>.")

  preset = parts[0]       # The part before the colon
  application = parts[1]  # The part after the colon

  return preset, application


def expand_penv_variables(path):
  """
  Expand environment variables in the form of $penv{VAR_NAME}.

  :param path: The input path string with $penv{...} placeholders.
  :return: The expanded path with environment variables replaced.
  """
  # Regular expression to find $penv{VAR_NAME}
  pattern = re.compile(r"\$penv\{(.*?)\}")

  # Function to replace each match with its environment variable value
  def replace_var(match):
    var_name = match.group(1)  # Extract the variable name
    return os.environ.get(var_name, "")  # Get the value or use an empty string if undefined

  # Substitute all occurrences of the pattern
  return pattern.sub(replace_var, path)


def expand_cmake_presets(presets_path):
  """
  Recursively expand and merge CMakePresets.json files.

  :param presets_path: Path to the initial CMakePresets.json file.
  :return: Merged dictionary of all included CMakePresets.json files.
  """
  presets_path = Path(presets_path)
  if not presets_path.exists():
    raise FileNotFoundError(f"Presets file not found: {presets_path}")

  with presets_path.open("r") as f:
    data = json.load(f)

  # Handle `include` field if present
  includes = data.get("include", [])
  merged_data = {
      "version": data.get("version", None),
      "cmakeMinimumRequired": data.get("cmakeMinimumRequired", {}),
      "configurePresets": data.get("configurePresets", []),
      "buildPresets": data.get("buildPresets", []),
  }

  for include_path in includes:
    resolved_path = expand_penv_variables(include_path)

    # Recursively expand the included file
    included_data = expand_cmake_presets(resolved_path)

    # Merge included presets into the main dictionary
    merged_data["configurePresets"].extend(included_data.get("configurePresets", []))
    merged_data["buildPresets"].extend(included_data.get("buildPresets", []))

  return merged_data


def resolve_bin_dir(preset_name: str) -> str:

  # First verify existence of CMakePresets.json.
  if not os.path.exists(CMAKE_PRESETS_JSON):
    forge.error(f"{CMAKE_PRESETS_JSON} does not exist!")

  # Then verify preset_name is a valid preset.
  check_preset(preset_name)

  try:
    merged_presets = expand_cmake_presets(CMAKE_PRESETS_JSON)
    binary_dir = next(
        (preset.get("binaryDir")
         for preset in merged_presets["configurePresets"] if preset["name"] == preset_name),
        None,
    )
  except Exception as e:
    forge.error(f"Error: {e}")

  if binary_dir is None:
    return forge.error(f"Preset<{preset_name}> does not set \"binaryDir\"!")
  else:
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
    print("\n\nApplication information:")
    print_size(self.application_fullfile)
    print("Running locally...")
    args = [self.application_fullfile]
    subprocess.check_call(args)
    forge.print_green("Complete!")

  def debug(self):
    debugger = forge.NativeDebugger(name=f"{self.preset_name}:{self.application_name}")
    debugger.debug(self.application_fullfile)
