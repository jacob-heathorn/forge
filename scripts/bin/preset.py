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

class Preset:
  def __init__(self, name: str, project_root: os.path):
    self.name = name
    self.project_root = project_root
    # TODO verify preset using cmake.
  
  def set_debugger(self, debugger):
    self.debugger = debugger

  def debug(self, executable: os.path):
    self.debugger.debug(executable)
