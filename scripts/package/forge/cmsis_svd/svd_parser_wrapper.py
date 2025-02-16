# System pythonmodules
import os
import shutil
from jinja2 import Template
import fnmatch

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


class SVDParserWrapper:
  def __init__(self, svd_file, output_dir):
    file = '/home/jacob/evtol/nxp/repos/mcux-sdk/svd/MIMXRT1176/MIMXRT1176_cm7.xml'
    self.svd_parser = SVDParser.for_xml_file(svd_file)
    self.output_dir = output_dir
    print("here1")
    self.device = self.svd_parser.get_device()
    print("here2")
    self.clean()

    template_file = os.path.join(FORGE_ROOT, 'scripts', 'templates', 'cmsis_svd_registers.jinja2')
    with open(template_file, 'r') as file:
      template_content = file.read()
      self.template = Template(template_content)

    # Start clean.
    self.clean()

    # Copy base Register source.
    shutil.copy(
        os.path.join(
            FORGE_ROOT,
            'scripts',
            'templates',
            'register_bit_manipulation.hpp'),
        output_dir)
    shutil.copy(os.path.join(FORGE_ROOT, 'scripts', 'templates', 'register32.hpp'), output_dir)

  def clean(self):
    clean_headers(self.output_dir)

  def generate_peripheral(self, peripheral_name: str):
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
