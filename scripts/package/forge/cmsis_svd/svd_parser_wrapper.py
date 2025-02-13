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
  def __init__(self, vendor: str, svd_filename: str):
    # self.svd_parser = SVDParser.for_packaged_svd(SVD_DATA_DIR, vendor, svd_filename)
    file = '/home/jacob/evtol/nxp/repos/mcux-sdk/svd/MIMXRT1176/MIMXRT1176_cm7.xml'
    self.svd_parser = SVDParser.for_xml_file(file)
    self.skip_patterns = []

  def skip(self, pattern):
    self.skip_patterns.append(pattern)

  def generate(self, output_dir: str):
    # First clean the outpud dir
    clean_headers(output_dir)

    # Read the template content
    template_file = os.path.join(FORGE_ROOT, 'scripts', 'templates', 'cmsis_svd_registers.jinja2')
    with open(template_file, 'r') as file:
      template_content = file.read()
      template = Template(template_content)

      for peripheral in self.svd_parser.get_device().peripherals:
        # Execute skip logic
        skip = False
        for pattern in self.skip_patterns:
          if fnmatch.fnmatch(peripheral.name.lower(), pattern.lower()):
            print(f"Skipping {peripheral.name}")
            skip = True
            break

        if skip:
          continue

        # TODO delete was using for debugging.
        # for register in peripheral.registers:
        #   for field in register.fields:
        #     if field.access == SVDAccessType.WRITE_ONLY:
        #       print("write only")
          # print(field.)

        # Create a Jinja Template instance with the content
        rendered_template = template.render(
            peripheral=peripheral,
            format_comment=format_comment,
            SVDAccessType=SVDAccessType)

        # Write the rendered template to a .hpp file
        peripheral_hpp = os.path.join(output_dir, f'{peripheral.name}.hpp'.lower())
        with open(peripheral_hpp, 'w') as f:
          f.write(rendered_template)

    # Copy base Register source
    shutil.copy(
        os.path.join(
            FORGE_ROOT,
            'scripts',
            'templates',
            'register_bit_manipulation.hpp'),
        output_dir)
    shutil.copy(os.path.join(FORGE_ROOT, 'scripts', 'templates', 'register32.hpp'), output_dir)
