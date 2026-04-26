import os
import forge

PROJECT_ROOT = os.environ.get("PROJECT_ROOT", "")
FORGE_ROOT = os.environ.get("FORGE_ROOT", "")
VSCODE_DIR = os.path.join(PROJECT_ROOT, ".vscode")
LAUNCH_JSON = os.path.join(VSCODE_DIR, "launch.json")
NATIVE_LAUNCH_TEMPLATE = os.path.join(
    FORGE_ROOT,
    'scripts',
    'templates',
    'native_launch_config.jinja2')
NATIVE_GDB_PATH = os.environ.get("NATIVE_GDB_PATH")


class NativeDebugger():
  """
  Provides the interface to generate a native launch configuration for vscode.
  """

  def __init__(self, name: str) -> None:
    self.name = name

  def debug(self, fullfile: str) -> None:
    launch_manager = forge.vscode.LaunchManager(LAUNCH_JSON)

    # Define the context for your template rendering
    context = {
        'name': self.name,
        'executable': fullfile,
        'gdb_path': NATIVE_GDB_PATH
    }

    launch_manager.update(NATIVE_LAUNCH_TEMPLATE, context)
    forge.print_green(f"In VSCode use run config: {self.name}")
