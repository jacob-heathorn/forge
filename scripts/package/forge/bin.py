import os

PROJECT_ROOT = os.environ.get("PROJECT_ROOT")
# BIN_ROOT = os.path.join(PROJECT_ROOT, 'bin')

import json


def discover_preset_build_directory(preset_name):
  presets_file = os.path.join(PROJECT_ROOT, "CMakePresets.json")

  with open(presets_file, "r") as f:
    data = json.load(f)

  binary_dir = next(
      (preset.get("binaryDir") for preset in data["configurePresets"] if preset["name"] == preset_name),
      None,
  )

  # Expand cmake environment variables.
  binary_dir = binary_dir.replace("${sourceDir}", os.path.dirname(presets_file))
  binary_dir = binary_dir.replace("${presetName}", preset_name)


  if binary_dir:
      print(f"Binary directory for preset '{preset_name}': {binary_dir}")
  else:
      print(f"Preset '{preset_name}' not found.")


def resolve_application(preset_application: str):
  # Split the string into two parts at the first colon
  parts = preset_application.split(':', 1)  # '1' is the maxsplit argument

  # Assign the parts to respective variables
  preset = parts[0]  # The part before the colon
  # The part after the colon, or None if no colon
  application = parts[1] if len(parts) > 1 else None

  discover_preset_build_directory(preset)

  # # Resolve
  # search_dir = os.path.join(BIN_ROOT, preset)
  # application_fullfile = find_application(application, search_dir)
  # print(f"Found: {preset}:{application_fullfile}")
  return preset, "TODO"
