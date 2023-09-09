#!/usr/bin/env python3
#
# Top-level repository build tool.

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

from helpers import print_green, print_red, Error, pushd

FORGE_ROOT = os.environ.get("FORGE_ROOT")

class Preset:
  def __init__(self, name: str, project_root: os.path):
    self.name = name
    self.project_root = project_root
    # TODO verify preset using cmake.

class GdbDebugger:
  def __init__(self, preset: Preset, gdb: os.path, template_file: os.path):
    self.preset = preset
    self.gdb = gdb
    self.template_file = template_file
    self.vscode_folder = os.path.join(preset.project_root, ".vscode")
    self.generated_launch_json = os.path.join(self.vscode_folder, "launch.json")
  
  def _generate_launch_json(self, executable: os.path):
    # Read the template content
    with open(self.template_file, 'r') as file:
        template_content = file.read()

    # Create a Jinja Template instance with the content
    template = Template(template_content)

    # Render the template
    display_name = f"{self.preset.name} (gdb)"
    rendered_template = template.render(name=display_name, executable=executable, gdb_path=self.gdb)

    # Create the .vscode directory if it doesn't exist
    if not os.path.exists(self.vscode_folder):
      os.makedirs(self.vscode_folder)

    # Write the rendered template to a file
    with open(self.generated_launch_json, 'w') as f:
      f.write(rendered_template)
    
    print(f"Generated {self.generated_launch_json}")

class NativeDebugger(GdbDebugger):
  def __init__(self, preset: Preset, native_gdb: os.path):
    native_template_file = os.path.join(FORGE_ROOT, "scripts", "launch-native.json.jinja2")
    super().__init__(preset, gdb=native_gdb, template_file=native_template_file)

  def debug(self, executable: os.path):
    self._generate_launch_json(executable)
    print_green("Start debugging in VSCode (F5)!")
