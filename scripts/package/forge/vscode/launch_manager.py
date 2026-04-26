import json5
import os
from jinja2 import Template
from typing import Dict, List, Any


class LaunchManager:
  """
  Provides an interface to manage the .vscode/launch.json file.
  """

  def __init__(self, file: str) -> None:
    self.file = file
    self.data = self._load_or_create_default()

  def _load_or_create_default(self) -> dict[str, Any]:
    """
    Loads the launch.json file or creates a default file.
    """
    if not os.path.exists(self.file):
      # File does not exist, create it with default configuration
      return self._create_default_file()
    else:
      # File exists, load its content
      with open(self.file, 'r') as file:
        try:
          data = json5.load(file)
        except ValueError:
          # Handle case where file is empty or contains invalid JSON5
          return self._create_default_file()

        # Check if 'configurations' key exists
        if 'configurations' not in data:
          # Add default configurations and save the file
          data['configurations'] = []
          with open(self.file, 'w') as file:
            json5.dump(data, file, indent=4)
        return data

  def _create_default_file(self) -> dict[str, Any]:
    """
    Generates a default launch.json file.
    """
    default_data: Dict[str, List[Any]] = {
        "configurations": []
    }
    with open(self.file, 'w') as file:
      json5.dump(default_data, file, indent=4)
    return default_data

  def update(self, template_fullfile: str, context: dict[str, Any]) -> None:
    """
    Adds a new configuration to the launch.json file.
    """
    # Read the template file
    with open(template_fullfile, 'r') as file:
      template_content = file.read()

    # Create a Template instance
    template = Template(template_content)

    # We assume that the template output will be valid JSON5, parse it as such
    new_config = json5.loads(template.render(context))

    # Replace the existing configuration if it exists
    for idx, config in enumerate(self.data['configurations']):
      if config['name'] == new_config['name']:
        print(f"Updating launch configuration {config['name']}.")
        self.data['configurations'][idx] = new_config
        break
    else:
      print(f"Adding new launch configuration {new_config['name']}.")
      self.data['configurations'].append(new_config)

    # Save
    with open(self.file, 'w', encoding='utf-8') as file:
      json5.dump(self.data, file, indent=2, quote_keys=True, trailing_commas=False)
