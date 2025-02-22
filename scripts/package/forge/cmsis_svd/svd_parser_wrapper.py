# System pythonmodules
import pickle
import os
import shutil
from jinja2 import Template

# https://github.com/cmsis-svd/cmsis-svd
from cmsis_svd.parser import SVDParser
from cmsis_svd.model import SVDAccessType
# import cmsis_svd

FORGE_ROOT = os.environ.get("FORGE_ROOT", "")
CMSIS_SVD_DATA_ROOT = os.environ.get("CMSIS_SVD_DATA_ROOT", "")
SVD_DATA_DIR = os.path.join(CMSIS_SVD_DATA_ROOT, 'data')

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

# TODO just for debugging
save_cache = False


class SVDParserWrapper:
  def __init__(self, svd_file, output_dir):
    file = '/home/jacob/evtol/nxp/repos/mcux-sdk/svd/MIMXRT1176/MIMXRT1176_cm7.xml'
    self.svd_parser = SVDParser.for_xml_file(svd_file)
    self.output_dir = output_dir

    # TODO just for debugging
    if save_cache:
      print("Parsing and saving device to cache")
      self.device = self.svd_parser.get_device()
      with open(os.path.join('.bin', 'device_cache.pkl'), 'wb') as f:
        pickle.dump(self.device, f)
      print("done saving.")
    else:
      print("Loading device from cache.")
      with open(os.path.join('.bin', 'device_cache.pkl'), 'rb') as f:
        self.device = pickle.load(f)
        print("done loading.")

    self.clean()

    template_file = os.path.join(FORGE_ROOT, 'scripts', 'templates', 'cmsis_svd_registers.jinja2')
    with open(template_file, 'r') as file:
      template_content = file.read()
      self.template = Template(template_content)

    # Start clean.
    self.clean()

  def clean(self):
    clean_headers(self.output_dir)

  def generate_peripheral(self, peripheral_name: str):
    generated = False
    all_names = []
    # Find the peripheral
    for peripheral in self.device.peripherals:
      all_names.append(peripheral.name)
      if peripheral.name == peripheral_name:

        for register in peripheral.registers:
          for field in register.fields:
            # TODO: in jinja, I am accessing [0] assuming there is only one set of
            # enum types for a field. This assumption could be wrong.
            if field.is_enumerated_type:
              print(f"field: {field.name}")
              if field.enumerated_values:
                list_enumerated_values = field.enumerated_values
                for enumerated_values in list_enumerated_values:
                  print(f"name: {enumerated_values.name}")
                  print(f"usage: {enumerated_values.usage}")
                  print(f"derived from: {enumerated_values.derived_from}")
                  print(f"header_enum_name: {enumerated_values.header_enum_name}")

                  for enumerated_value in enumerated_values.enumerated_values:
                    print(f"  {enumerated_value.name}")
                    print(f"  {enumerated_value.description}")
                    print(f"  {enumerated_value.value}")

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

  def generate(self):
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
