import json5
import os
from typing import Any
from jinja2 import Template


class TasksManager:
  """
  Provides an interface to manage the .vscode/tasks.json file.
  """

  def __init__(self, file: str) -> None:
    self.file = file
    self.data = self._load_or_create_default()

  def _load_or_create_default(self) -> dict[str, Any]:
    """
    Loads the tasks.json file or creates a default file.
    """
    if not os.path.exists(self.file):
      # File does not exist, create it with default task
      return self._create_default_file()
    else:
      # File exists, load its content
      with open(self.file, 'r') as file:
        try:
          data = json5.load(file)
        except ValueError:
          # Handle case where file is empty or contains invalid JSON5
          return self._create_default_file()

        # Check if 'tasks' key exists
        if 'tasks' not in data:
          # Add default tasks and save the file
          data['tasks'] = []
          with open(self.file, 'w') as file:
            json5.dump(data, file, indent=4)
        return data

  def _create_default_file(self) -> dict[str, Any]:
    """
    Generates a default tasks.json file.
    """
    default_data = {
        "version": "2.0.0",
        "tasks": []
    }
    with open(self.file, 'w') as file:
      json5.dump(default_data, file, indent=4)
    return default_data

  def update(self, template_fullfile: str, context: dict[str, Any]) -> None:
    """
    Adds a new configuration to the tasks.json file.
    """
    # Read the template file
    with open(template_fullfile, 'r') as file:
      template_content = file.read()

    # Create a Template instance
    template = Template(template_content)

    # We assume that the template output will be valid JSON5, parse it as such
    new_task = json5.loads(template.render(context))

    # Replace the existing task if it exists
    for idx, task in enumerate(self.data['tasks']):
      if task['label'] == new_task['label']:
        print(f"Updating task {task['label']}.")
        self.data['tasks'][idx] = new_task
        break
    else:
      print(f"Adding new task: {new_task['label']}.")
      self.data['tasks'].append(new_task)

    # Save
    with open(self.file, 'w', encoding='utf-8') as file:
      json5.dump(self.data, file, indent=2, quote_keys=True, trailing_commas=False)
