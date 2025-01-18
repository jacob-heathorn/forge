import contextlib
import os

GREEN_CHECK = "\033[32m\u2713\033[0m"

# Prints green


def print_green(text):
  print(f"\033[32m{text}\033[0m")

# Prints red


def print_red(text):
  print(f"\033[91m{text}\033[0m")

# Prints and raises an exception


def error(message: str):
  print_red(message)
  raise (Exception(message))

# Erros if path/directory DNE


def ensure_path(path: os.path):
  if not os.path.exists(path):
    error(f'path<{path}> DNE!')

# Erros if path/directory DNE


def ensure_file(fullfile: os.path):
  if not os.path.exists(fullfile):
    error(f'file<{fullfile}> DNE!')


def remove_file(fullfile):
  if os.path.exists(fullfile):
    os.remove(fullfile)


# Context manager for pushd. Example from
# (https://stackoverflow.com/questions/6194499/pushd-through-os-system)
@contextlib.contextmanager
def pushd(new_dir):
  previous_dir = os.getcwd()

  # Create the directoy if it doesn't exsits (not exactly pushd)
  if not os.path.exists(new_dir):
    os.makedirs(new_dir)

  os.chdir(new_dir)
  try:
    yield
  finally:
    os.chdir(previous_dir)
