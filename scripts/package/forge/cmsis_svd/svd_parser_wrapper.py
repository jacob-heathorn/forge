# System pythonmodules
import pickle
import os
from jinja2 import Environment, FileSystemLoader

# https://github.com/cmsis-svd/cmsis-svd
from cmsis_svd.parser import SVDParser
from cmsis_svd.model import SVDAccessType

FORGE_ROOT = os.environ.get("FORGE_ROOT", "")
PROJECT_ROOT = os.environ.get("PROJECT_ROOT", "")
BIN_DIR = os.path.join(PROJECT_ROOT, '.bin')

# =================================================================================================
# Helpers

# Deletes all the header files in a directory


def clean_headers(dir: str):
  for filename in os.listdir(dir):
    file_path = os.path.join(dir, filename)
    if file_path.endswith('.hpp'):
      os.remove(file_path)


def format_comment(comment, width=100, indent_width=0):
  words = comment.split()

  # The line starts with indent, then "// " which takes up 3 characters
  line_width = 3 + indent_width
  lines = []
  current_line = []
  indent = ' ' * indent_width

  for word in words:
    # +1 for the space
    if line_width + len(word) + 1 <= width:
      current_line.append(word)
      line_width += len(word) + 1
    else:
      lines.append(indent + "// " + " ".join(current_line))
      current_line = [word]
      line_width = 3 + indent_width + len(word) + 1

  if current_line:
    lines.append(indent + "// " + " ".join(current_line))

  return "\n".join(lines)


# =================================================================================================
# CMSIS-SVD parser wrapper


class SVDParserWrapper:
  """
  Provides an interface to the cmsis_svd parser to generate peripheral register header files.
  """

  def __init__(self, svd_file, output_dir):
    self.svd_parser = SVDParser.for_xml_file(svd_file)
    self.output_dir = output_dir

    # Extract just the filename from the full path (without extension)
    svd_filename = os.path.splitext(os.path.basename(svd_file))[0]
    cache_path = os.path.join(BIN_DIR, f"{svd_filename}.pkl")  # Cached file path

    # Save the devide object to the .bin directory and reload from there to save time during
    # rapid iterative developent.
    if os.path.exists(cache_path):
      print(f"Loading device from: {cache_path}")
      with open(cache_path, 'rb') as f:
        self.device = pickle.load(f)
      print("Done loading from cache.")
    else:
      print(f"Parsing and saving device to {cache_path}")
      self.svd_parser = SVDParser.for_xml_file(svd_file)
      self.device = self.svd_parser.get_device()

      os.makedirs(BIN_DIR, exist_ok=True)

      with open(cache_path, 'wb') as f:
        pickle.dump(self.device, f)
      print("Done saving to cache.")

    # Load template dir.
    template_dir = os.path.join(FORGE_ROOT, 'scripts', 'templates')
    self.env = Environment(loader=FileSystemLoader(template_dir))
    self.template = self.env.get_template('cmsis_svd_registers.jinja2')

    # Start clean.
    clean_headers(self.output_dir)

  def generate_peripheral(self, peripheral_name: str):
    """
    Generates the register definitions for a single peripheral.
    """
    generated = False
    all_names = []
    # Find the peripheral
    for peripheral in self.device.peripherals:
      all_names.append(peripheral.name)
      if peripheral.name == peripheral_name:

        # Create a Jinja Template instance with the content
        rendered_template = self.template.render(
            peripheral=peripheral,
            format_comment=format_comment,
            SVDAccessType=SVDAccessType)

        # Write the rendered template to a .hpp file
        peripheral_hpp = os.path.join(self.output_dir, f'{peripheral.name}.hpp'.lower())
        with open(peripheral_hpp, 'w') as f:
          f.write(rendered_template)

        generated = True

    if generated is False:
      print(f"Could not find peripheral, possible options include:\n{all_names}")
    else:
      print(f"Register definitions generated in: {self.output_dir} for {peripheral_name}")

  def generate(self):
    """
    Generates the register definitions for ALL peripherals.
    """
    for peripheral in self.device.peripherals:
      # Create a Jinja Template instance with the content
      rendered_template = self.template.render(
          peripheral=peripheral,
          format_comment=format_comment,
          SVDAccessType=SVDAccessType)

      # Write the rendered template to a .hpp file
      peripheral_hpp = os.path.join(self.output_dir, f'{peripheral.name}.hpp'.lower())
      with open(peripheral_hpp, 'w') as f:
        f.write(rendered_template)

    print(f"Register definitions generated in: {self.output_dir}")
