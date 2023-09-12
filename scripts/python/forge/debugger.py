import os
from jinja2 import Template
import subprocess

from forge.helpers import print_green, ensure_file

FORGE_ROOT = os.environ.get("FORGE_ROOT")

class GdbDebugger:
  def __init__(self, name: str, project_root: str, gdb: os.path, template_file: os.path):
    self.name = name
    self.project_root = project_root
    self.gdb = gdb
    self.template_file = template_file
    self.vscode_folder = os.path.join(project_root, ".vscode")
    self.generated_launch_json = os.path.join(self.vscode_folder, "launch.json")
  
  def _generate_launch_json(self, executable: os.path):
    # Read the template content
    with open(self.template_file, 'r') as file:
        template_content = file.read()

    # Create a Jinja Template instance with the content
    template = Template(template_content)

    # Render the template
    display_name = f"{self.name} (gdb)"
    rendered_template = template.render(name=display_name, executable=executable, gdb_path=self.gdb)

    # Create the .vscode directory if it doesn't exist
    if not os.path.exists(self.vscode_folder):
      os.makedirs(self.vscode_folder)

    # Write the rendered template to a file
    with open(self.generated_launch_json, 'w') as f:
      f.write(rendered_template)
    
    print(f"Generated {self.generated_launch_json}")

class NativeDebugger(GdbDebugger):
  def __init__(self, name: str, project_root: str, gdb: os.path):
    native_template_file = os.path.join(FORGE_ROOT, 'scripts', 'launch-native.json.jinja2')
    ensure_file(native_template_file)
    super().__init__(name, project_root, gdb, native_template_file)

  def debug(self, fullfile: os.path):
    self._generate_launch_json(fullfile)
    print_green("Start debugging in VSCode (F5)!")

  def run(self, fullfile: os.path):
    print_green(f"Runnig executable {fullfile}")
    args = [fullfile]
    subprocess.check_call(args)
    print_green("Success!")
